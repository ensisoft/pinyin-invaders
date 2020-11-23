// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QStringList>
#  include <QFileDialog>
#  include <QMessageBox>
#  include <QEventLoop>
#  include <QDir>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "editor/app/packing.h"
#include "editor/app/utility.h"
#include "editor/gui/utility.h"
#include "dlgpackage.h"

namespace gui
{

DlgPackage::DlgPackage(QWidget* parent, app::Workspace& workspace)
    : QDialog(parent)
    , mWorkspace(workspace)
{
    mUI.setupUi(this);

    const auto num_resources = mWorkspace.GetNumResources();
    for (size_t i=0; i<num_resources; ++i)
    {
        const auto& resource = mWorkspace.GetResource(i);
        if (resource.IsPrimitive())
            continue;
        const bool check = resource.GetProperty("checked_for_packing", true);
        QListWidgetItem* item = new QListWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(check ? Qt::Checked : Qt::Unchecked);
        item->setIcon(resource.GetIcon());
        item->setText(resource.GetName());
        item->setData(Qt::UserRole, quint64(i));
        mUI.listWidget->addItem(item);
    }

    GetProperty(workspace, "packing_param_max_tex_height", mUI.cmbMaxTexHeight);
    GetProperty(workspace, "packing_param_max_tex_width", mUI.cmbMaxTexWidth);
    GetProperty(workspace, "packing_param_combine_textures", mUI.chkCombineTextures);
    GetProperty(workspace, "packing_param_resize_large_textures", mUI.chkResizeTextures);
    GetProperty(workspace, "packing_param_pack_name", mUI.editPckName);
    GetProperty(workspace, "packing_param_write_config", mUI.chkWriteConfig);
    GetProperty(workspace, "packing_param_write_content", mUI.chkWriteContent);

    SetValue(mUI.editOutDir, app::CleanPath(workspace.GetDir()));
    mUI.progressBar->setVisible(false);

    connect(&mWorkspace, &app::Workspace::ResourcePackingUpdate,
            this, &DlgPackage::ResourcePackingUpdate);
}

void DlgPackage::on_btnSelectAll_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Checked);
    }
}

void DlgPackage::on_btnSelectNone_clicked()
{
    for (int i=0; i<GetCount(mUI.listWidget); ++i)
    {
        QListWidgetItem* item = mUI.listWidget->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}

void DlgPackage::on_btnBrowse_clicked()
{
    const auto& dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"), mWorkspace.GetDir());
    if (dir.isEmpty())
        return;

    mUI.editOutDir->setText(dir);
}
void DlgPackage::on_btnStart_clicked()
{
    if (!MustHaveInput(mUI.editOutDir))
        return;
    else if (!MustHaveInput(mUI.editPckName))
        return;
    else if (!MustHaveNumber(mUI.cmbMaxTexHeight))
        return;
    else if (!MustHaveNumber(mUI.cmbMaxTexWidth))
        return;

    const QString& dir  = GetValue(mUI.editOutDir);
    const QString& name = GetValue(mUI.editPckName);
    const auto& out  = app::JoinPath(dir, name);
    if (!QDir(out).isEmpty())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The directory\n%1\ncontains files that might get overwritten.\n"
            "Are you sure you want to proceed?").arg(out));
        if (msg.exec() == QMessageBox::No)
            return;
    }
    mUI.btnStart->setEnabled(false);
    mUI.btnClose->setEnabled(false);
    mUI.progressBar->setVisible(true);
    mPackageInProgress = true;

    std::vector<const app::Resource*> resources;

    for (size_t i=0; i<GetCount(mUI.listWidget); ++i)
    {
        const QListWidgetItem* item = mUI.listWidget->item(i);
        const auto index   = item->data(Qt::UserRole).toULongLong();
        const auto checked = item->checkState() == Qt::Checked;
        auto& resource = mWorkspace.GetResource(index);
        resource.SetProperty("checked_for_packing", checked);
        if (checked)
        {
            resources.push_back(&resource);
        }
    }
    // remember the settings.
    SetProperty(mWorkspace, "packing_param_max_tex_height", mUI.cmbMaxTexHeight);
    SetProperty(mWorkspace, "packing_param_max_tex_width", mUI.cmbMaxTexWidth);
    SetProperty(mWorkspace, "packing_param_combine_textures", mUI.chkCombineTextures);
    SetProperty(mWorkspace, "packing_param_resize_large_textures", mUI.chkResizeTextures);
    SetProperty(mWorkspace, "packing_param_pack_name", name);
    SetProperty(mWorkspace, "packing_param_write_config", mUI.chkWriteConfig);
    SetProperty(mWorkspace, "packing_param_write_content", mUI.chkWriteContent);

    app::Workspace::ContentPackingOptions options;
    options.directory          = dir;
    options.package_name       = name;
    options.combine_textures   = GetValue(mUI.chkCombineTextures);
    options.resize_textures    = GetValue(mUI.chkResizeTextures);
    options.max_texture_width  = GetValue(mUI.cmbMaxTexWidth);
    options.max_texture_height = GetValue(mUI.cmbMaxTexHeight);
    options.write_config_file  = GetValue(mUI.chkWriteConfig);
    options.write_content_file = GetValue(mUI.chkWriteContent);

    const auto success = mWorkspace.PackContent(resources, options);

    mUI.btnStart->setEnabled(true);
    mUI.btnClose->setEnabled(true);
    mUI.progressBar->setVisible(false);
    mPackageInProgress = false;

    if (success)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Success!\nHave a good day. :)"));
        msg.exec();
    }
    else
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Content packing completed with errors/warnings.\n"
                       "Please see the log for details."));
        msg.exec();
    }
}

void DlgPackage::on_btnClose_clicked()
{
    close();
}

void DlgPackage::closeEvent(QCloseEvent* event)
{
    if (mPackageInProgress)
    {
        event->ignore();
        return;
    }
    event->accept();
}

void DlgPackage::ResourcePackingUpdate(const QString& action, int step, int max)
{
    if (step > max)
        step = max;
    mUI.progressBar->setValue(step);
    mUI.progressBar->setMaximum(max);
    mUI.progressBar->setFormat(QString("%1 %p%").arg(action));

    QEventLoop footgun;
    footgun.processEvents();
}

} // namespace


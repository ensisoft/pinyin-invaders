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
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QDir>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "dlgsettings.h"
#include "utility.h"

namespace gui
{

DlgSettings::DlgSettings(QWidget* parent, AppSettings& settings)
    : QDialog(parent)
    , mSettings(settings)
{
    mUI.setupUi(this);
    mUI.edtImageEditorExecutable->setText(settings.image_editor_executable);
    mUI.edtImageEditorArguments->setText(settings.image_editor_arguments);
    mUI.edtShaderEditorExecutable->setText(settings.shader_editor_executable);
    mUI.edtShaderEditorArguments->setText(settings.shader_editor_arguments);

    SetValue(mUI.cmbTargetFps, settings.target_fps);
    SetValue(mUI.cmbMSAA, settings.msaa_sample_count);
    SetValue(mUI.chkVSync, settings.sync_to_vblank);
    SetValue(mUI.cmbWinOrTab, settings.default_open_win_or_tab);
}

void DlgSettings::on_btnAccept_clicked()
{
     if (!MustHaveNumber(mUI.cmbTargetFps))
        return;

    mSettings.image_editor_executable = mUI.edtImageEditorExecutable->text();
    mSettings.image_editor_arguments  = mUI.edtImageEditorArguments->text();
    mSettings.shader_editor_executable = mUI.edtShaderEditorExecutable->text();
    mSettings.shader_editor_arguments  = mUI.edtShaderEditorArguments->text();
    mSettings.target_fps = GetValue(mUI.cmbTargetFps);
    mSettings.msaa_sample_count = GetValue(mUI.cmbMSAA);
    mSettings.sync_to_vblank = GetValue(mUI.chkVSync);
    mSettings.default_open_win_or_tab = (QString)GetValue(mUI.cmbWinOrTab);
    accept();
}
void DlgSettings::on_btnCancel_clicked()
{
    reject();
}

void DlgSettings::on_btnSelectImageEditor_clicked()
{
    QString filter;

    #if defined(WINDOWS_OS)
        filter = "Executables (*.exe)";
    #endif

    const QString& executable = QFileDialog::getOpenFileName(this,
        tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtImageEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtImageEditorExecutable->setCursorPosition(0);
}

void DlgSettings::on_btnSelectShaderEditor_clicked()
{
    QString filter;

    #if defined(WINDOWS_OS)
        filter = "Executables (*.exe)";
    #endif

    const QString& executable = QFileDialog::getOpenFileName(this,
        tr("Select Application"), QString(), filter);
    if (executable.isEmpty())
        return;

    const QFileInfo info(executable);
    mUI.edtShaderEditorExecutable->setText(QDir::toNativeSeparators(executable));
    mUI.edtShaderEditorExecutable->setCursorPosition(0);
}

} // namespace


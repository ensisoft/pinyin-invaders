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
#include "editor/gui/dlgproject.h"
#include "editor/gui/utility.h"
#include "graphics/device.h"

namespace gui
{

DlgProject::DlgProject(QWidget* parent, app::Workspace& workspace, app::Workspace::ProjectSettings& settings)
    : QDialog(parent)
    , mWorkspace(workspace)
    , mSettings(settings)
{
    const QString& library = mSettings.GetApplicationLibrary();
    mUI.setupUi(this);
    PopulateFromEnum<gfx::Device::MinFilter>(mUI.cmbMinFilter);
    PopulateFromEnum<gfx::Device::MagFilter>(mUI.cmbMagFilter);
    PopulateFromEnum<app::Workspace::ProjectSettings::WindowMode>(mUI.cmbWindowMode);
    PopulateFromEnum<app::Workspace::ProjectSettings::GameEngine>(mUI.cmbEngine);
    SetUIValue(mUI.cmbMSAA, mSettings.multisample_sample_count);
    SetUIValue(mUI.cmbMinFilter, mSettings.default_min_filter);
    SetUIValue(mUI.cmbMagFilter, mSettings.default_mag_filter);
    SetUIValue(mUI.cmbWindowMode, mSettings.window_mode);
    SetUIValue(mUI.wndWidth, mSettings.window_width);
    SetUIValue(mUI.wndHeight, mSettings.window_height);
    SetUIValue(mUI.chkWndCanResize, mSettings.window_can_resize);
    SetUIValue(mUI.chkWndHasBorder, mSettings.window_has_border);
    SetUIValue(mUI.chkVsync, mSettings.window_vsync);
    SetUIValue(mUI.edtAppName, mSettings.application_name);
    SetUIValue(mUI.edtAppVersion, mSettings.application_version);
    SetUIValue(mUI.edtAppLibrary, library);
    SetUIValue(mUI.ticksPerSecond, mSettings.ticks_per_second);
    SetUIValue(mUI.updatesPerSecond, mSettings.updates_per_second);
    SetUIValue(mUI.edtWorkingFolder, mSettings.working_folder);
    SetUIValue(mUI.edtArguments, mSettings.command_line_arguments);
    SetUIValue(mUI.chkGameProcess, mSettings.use_gamehost_process);
    SetUIValue(mUI.numVeloIterations, mSettings.num_velocity_iterations);
    SetUIValue(mUI.numPosIterations, mSettings.num_position_iterations);
    SetUIValue(mUI.gravityX, mSettings.gravity.x);
    SetUIValue(mUI.gravityY, mSettings.gravity.y);
    SetUIValue(mUI.scaleX, mSettings.physics_scale.x);
    SetUIValue(mUI.scaleY, mSettings.physics_scale.y);
    SetUIValue(mUI.viewportWidth, mSettings.viewport_width);
    SetUIValue(mUI.viewportHeight, mSettings.viewport_height);
    SetUIValue(mUI.clearColor, mSettings.clear_color);
}

void DlgProject::on_btnAccept_clicked()
{
    GetUIValue(mUI.cmbMSAA, &mSettings.multisample_sample_count);
    GetUIValue(mUI.cmbMinFilter, &mSettings.default_min_filter);
    GetUIValue(mUI.cmbMagFilter, &mSettings.default_mag_filter);
    GetUIValue(mUI.wndWidth, &mSettings.window_width);
    GetUIValue(mUI.wndHeight, &mSettings.window_height);
    GetUIValue(mUI.cmbWindowMode, &mSettings.window_mode);
    GetUIValue(mUI.chkWndCanResize, &mSettings.window_can_resize);
    GetUIValue(mUI.chkWndHasBorder, &mSettings.window_has_border);
    GetUIValue(mUI.chkVsync, &mSettings.window_vsync);
    GetUIValue(mUI.edtAppName, &mSettings.application_name);
    GetUIValue(mUI.edtAppVersion, &mSettings.application_version);
    GetUIValue(mUI.ticksPerSecond, &mSettings.ticks_per_second);
    GetUIValue(mUI.updatesPerSecond, &mSettings.updates_per_second);
    GetUIValue(mUI.edtWorkingFolder, &mSettings.working_folder);
    GetUIValue(mUI.edtArguments, &mSettings.command_line_arguments);
    GetUIValue(mUI.chkGameProcess, &mSettings.use_gamehost_process);
    GetUIValue(mUI.numVeloIterations, &mSettings.num_velocity_iterations);
    GetUIValue(mUI.numPosIterations, &mSettings.num_position_iterations);
    GetUIValue(mUI.gravityX, &mSettings.gravity.x);
    GetUIValue(mUI.gravityY, &mSettings.gravity.y);
    GetUIValue(mUI.scaleX, &mSettings.physics_scale.x);
    GetUIValue(mUI.scaleY, &mSettings.physics_scale.y);
    GetUIValue(mUI.viewportWidth, &mSettings.viewport_width);
    GetUIValue(mUI.viewportHeight, &mSettings.viewport_height);
    GetUIValue(mUI.clearColor, &mSettings.clear_color);
    QString library;
    GetUIValue(mUI.edtAppLibrary, &library);
    mSettings.SetApplicationLibrary(library);
    accept();
}
void DlgProject::on_btnCancel_clicked()
{
    reject();
}
void DlgProject::on_btnSelectEngine_clicked()
{
#if defined(POSIX_OS)
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Engine Library"), "", tr("Library files (*.so)"));
#elif defined(WINDOWS_OS)
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Engine Library"), "", tr("Library files (*.dll)"));
#endif
    if (list.isEmpty())
        return;
    const auto& file = mWorkspace.AddFileToWorkspace(list[0]);
    SetValue(mUI.edtAppLibrary, file);
}

void DlgProject::on_cmbEngine_currentIndexChanged(const QString&)
{
    QString library;
#if defined(POSIX_OS)
    library = "app://libGameEngine.so";
#elif defined(WINDOWS_OS)
    library = "app://GameEngine.dll";
#endif

    const app::Workspace::ProjectSettings::GameEngine engine = GetValue(mUI.cmbEngine);
    if (engine == app::Workspace::ProjectSettings::GameEngine::Default)
    {
        SetValue(mUI.edtAppLibrary, library);
        SetEnabled(mUI.btnSelectEngine, false);
    }
    else
    {
        SetEnabled(mUI.btnSelectEngine, true);
    }

}

} // namespace


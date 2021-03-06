// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define LOGTAG "mainwindow"

#include "config.h"

#include "warnpush.h"
#  include <QGuiApplication>
#  include <QScreen>
#  include <QMessageBox>
#  include <QFileInfo>
#  include <QWheelEvent>
#  include <QFileInfo>
#  include <QStringList>
#  include <QProcess>
#  include <QSurfaceFormat>
#include "warnpop.h"

#if defined(__GCC__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <algorithm>

#include "base/assert.h"
#include "data/json.h"
#include "graphics/resource.h"
#include "editor/app/format.h"
#include "editor/app/utility.h"
#include "editor/app/eventlog.h"
#include "editor/gui/mainwindow.h"
#include "editor/gui/mainwidget.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/playwindow.h"
#include "editor/gui/settings.h"
#include "editor/gui/particlewidget.h"
#include "editor/gui/materialwidget.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/scenewidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/polygonwidget.h"
#include "editor/gui/dlgabout.h"
#include "editor/gui/dlgsettings.h"
#include "editor/gui/dlgimgpack.h"
#include "editor/gui/dlgpackage.h"
#include "editor/gui/dlgopen.h"
#include "editor/gui/dlgnew.h"
#include "editor/gui/dlgproject.h"
#include "editor/gui/dlgsave.h"
#include "editor/gui/utility.h"
#include "editor/gui/gfxwidget.h"
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/codewidget.h"
#include "editor/gui/uiwidget.h"

namespace {
// returns number of seconds elapsed since the last call
// of this function.
double ElapsedSeconds()
{
    using clock = std::chrono::steady_clock;
    static auto start = clock::now();
    auto now  = clock::now();
    auto gone = now - start;
    start = now;
    return std::chrono::duration_cast<std::chrono::microseconds>(gone).count() /
        (1000.0 * 1000.0);
}

class IterateGameLoopEvent : public QEvent
{
public:
    IterateGameLoopEvent() : QEvent(GetIdentity())
    {}
    static QEvent::Type GetIdentity()
    {
        static auto id = QEvent::registerEventType();
        return (QEvent::Type)id;
    }
private:
};

gui::MainWidget* CreateWidget(app::Resource::Type type, app::Workspace* workspace, const app::Resource* resource = nullptr)
{
    switch (type) {
        case app::Resource::Type::Material:
            if (resource)
                return new gui::MaterialWidget(workspace, *resource);
            return new gui::MaterialWidget(workspace);
        case app::Resource::Type::ParticleSystem:
            if (resource)
                return new gui::ParticleEditorWidget(workspace, *resource);
            return new gui::ParticleEditorWidget(workspace);
        case app::Resource::Type::Shape:
            if (resource)
                return new gui::ShapeWidget(workspace, *resource);
            return new gui::ShapeWidget(workspace);
        case app::Resource::Type::Entity:
            if (resource)
                return new gui::EntityWidget(workspace, *resource);
            return new gui::EntityWidget(workspace);
        case app::Resource::Type::Scene:
            if (resource)
                return new gui::SceneWidget(workspace, *resource);
            return new gui::SceneWidget(workspace);
        case app::Resource::Type::Script:
            if (resource)
                return new gui::ScriptWidget(workspace, *resource);
            return new gui::ScriptWidget(workspace);
        case app::Resource::Type::UI:
            if (resource)
                return new gui::UIWidget(workspace, *resource);
            return new gui::UIWidget(workspace);
    }
    BUG("Unhandled widget type.");
    return nullptr;
}

gui::MainWidget* MakeWidget(app::Resource::Type type, app::Workspace* workspace, const app::Resource* resource = nullptr)
{
    auto* widget = CreateWidget(type, workspace, resource);
    if (resource)
        widget->SetId(resource->GetId());
    return widget;
}

} // namespace

namespace gui
{

MainWindow::MainWindow(QApplication& app) : mApplication(app)
{
    mUI.setupUi(this);
    mUI.actionExit->setShortcut(QKeySequence::Quit);
    mUI.actionWindowClose->setShortcut(QKeySequence::Close);
    mUI.actionWindowNext->setShortcut(QKeySequence::Forward);
    mUI.actionWindowPrev->setShortcut(QKeySequence::Back);
    mUI.statusbar->insertPermanentWidget(0, mUI.statusBarFrame);
    mUI.statusbar->setVisible(true);
    mUI.mainToolBar->setVisible(true);
    mUI.actionViewToolbar->setChecked(true);
    mUI.actionViewStatusbar->setChecked(true);
    mUI.actionCut->setShortcut(QKeySequence::Cut);
    mUI.actionCopy->setShortcut(QKeySequence::Copy);
    mUI.actionPaste->setShortcut(QKeySequence::Paste);
    mUI.actionUndo->setShortcut(QKeySequence::Undo);

    ShowHelpWidget();

    // start periodic refresh timer. this is low frequency timer that is used
    // to update the widget UI if needed, such as change the icon/window title
    // and tick the workspace for periodic cleanup and stuff.
    QObject::connect(&mRefreshTimer, &QTimer::timeout, this, &MainWindow::RefreshUI);
    mRefreshTimer.setInterval(500);
    mRefreshTimer.start();

    auto& events = app::EventLog::get();
    QObject::connect(&events, SIGNAL(newEvent(const app::Event&)),this, SLOT(ShowNote(const app::Event&)));
    mEventLog.SetModel(&events);
    mEventLog.setSourceModel(&events);
    mUI.eventlist->setModel(&mEventLog);

    setWindowTitle(QString("%1").arg(APP_TITLE));
    setAcceptDrops(true);

    QCoreApplication::postEvent(this, new IterateGameLoopEvent);
}

MainWindow::~MainWindow()
{}

void MainWindow::closeWidget(MainWidget* widget)
{
    const auto index = mUI.mainTab->indexOf(widget);
    if (index == -1)
        return;
    mUI.mainTab->removeTab(index);
}

void MainWindow::loadState()
{
    // Load master settings.
    Settings settings("Ensisoft", APP_TITLE);

    const auto log_bits       = settings.getValue("MainWindow", "log_bits", mEventLog.GetShowBits());
    const auto window_xdim    = settings.getValue("MainWindow", "width",  width());
    const auto window_ydim    = settings.getValue("MainWindow", "height", height());
    const auto window_xpos    = settings.getValue("MainWindow", "xpos", x());
    const auto window_ypos    = settings.getValue("MainWindow", "ypos", y());
    const auto show_statbar   = settings.getValue("MainWindow", "show_statusbar", true);
    const auto show_toolbar   = settings.getValue("MainWindow", "show_toolbar", true);
    const auto show_eventlog  = settings.getValue("MainWindow", "show_event_log", true);
    const auto show_workspace = settings.getValue("MainWindow", "show_workspace", true);
    const auto& dock_state    = settings.getValue("MainWindow", "toolbar_and_dock_state", QByteArray());
    settings.getValue("MainWindow", "recent_workspaces", &mRecentWorkspaces);
    settings.getValue("Settings", "image_editor_executable",  &mSettings.image_editor_executable);
    settings.getValue("Settings", "image_editor_arguments",   &mSettings.image_editor_arguments);
    settings.getValue("Settings", "shader_editor_executable", &mSettings.shader_editor_executable);
    settings.getValue("Settings", "shader_editor_arguments",  &mSettings.shader_editor_arguments);
    settings.getValue("Settings", "script_editor_executable", &mSettings.script_editor_executable);
    settings.getValue("Settings", "script_editor_arguments",  &mSettings.script_editor_arguments);
    settings.getValue("Settings", "default_open_win_or_tab",  &mSettings.default_open_win_or_tab);
    settings.getValue("Settings", "style_name", &mSettings.style_name);
    settings.getValue("Settings", "save_automatically_on_play", &mSettings.save_automatically_on_play);

    TextEditor::Settings editor_settings;
    settings.getValue("TextEditor", "font",                   &editor_settings.font_description);
    settings.getValue("TextEditor", "font_size",              &editor_settings.font_size);
    settings.getValue("TextEditor", "theme",                  &editor_settings.theme);
    settings.getValue("TextEditor", "show_line_numbers",      &editor_settings.show_line_numbers);
    settings.getValue("TextEditor", "highlight_syntax",       &editor_settings.highlight_syntax);
    settings.getValue("TextEditor", "highlight_current_line", &editor_settings.highlight_current_line);
    settings.getValue("TextEditor", "insert_spaces",          &editor_settings.insert_spaces);
    settings.getValue("TextEditor", "tab_spaces",             &editor_settings.tab_spaces);
    TextEditor::SetDefaultSettings(editor_settings);

    QStyle* style = QApplication::setStyle(mSettings.style_name);
    if (style == nullptr) {
        WARN("No such application style '%1'", mSettings.style_name);
    } else {
        QApplication::setPalette(style->standardPalette());
        DEBUG("Application style set to '%1'", mSettings.style_name);
    }

    mEventLog.SetShowBits(log_bits);
    mEventLog.invalidate();
    mUI.actionLogShowInfo->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Info));
    mUI.actionLogShowWarning->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Warning));
    mUI.actionLogShowError->setChecked(mEventLog.IsShown(app::EventLogProxy::Show::Error));

    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();
    // try not to reposition the application to an offset that is not
    // within the current visible area
    if (window_xpos < size.width() && window_ypos < size.height())
        move(window_xpos, window_ypos);

    resize(window_xdim, window_ydim);

    mUI.mainToolBar->setVisible(show_toolbar);
    mUI.statusbar->setVisible(show_statbar);
    mUI.eventlogDock->setVisible(show_eventlog);
    mUI.workspaceDock->setVisible(show_workspace);
    mUI.actionViewToolbar->setChecked(show_toolbar);
    mUI.actionViewStatusbar->setChecked(show_statbar);
    mUI.actionViewEventlog->setChecked(show_eventlog);
    mUI.actionViewWorkspace->setChecked(show_workspace);

    if (!dock_state.isEmpty())
        QMainWindow::restoreState(dock_state);

    mUI.actionSaveWorkspace->setEnabled(false);
    mUI.actionCloseWorkspace->setEnabled(false);
    mUI.actionSelectResourceForEditing->setEnabled(false);
    mUI.menuWorkspace->setEnabled(false);
    mUI.menuEdit->setEnabled(false);
    mUI.menuTemp->setEnabled(false);
    mUI.workspace->setModel(nullptr);
    mWorkspaceProxy.SetModel(nullptr);
    mWorkspaceProxy.setSourceModel(nullptr);

    BuildRecentWorkspacesMenu();

    // check if we have a flag to disable workspace loading.
    // useful for development purposes when you know the workspace
    // might not load properly.
    const QStringList& args = QCoreApplication::arguments();
    for (const QString& arg : args)
    {
        if (arg == "--no-workspace")
            return;
    }

    // load previous workspace if any
    const auto& workspace = settings.getValue("MainWindow", "current_workspace", QString(""));
    if (workspace.isEmpty())
        return;

    if (!LoadWorkspace(workspace))
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("There was a problem loading the previous workspace."
                                        "\n'%1'\n"
                        "See the application log for more details.").arg(workspace));
        msg.exec();
    }
}

void MainWindow::focusWidget(const MainWidget* widget)
{
    const auto index = mUI.mainTab->indexOf(const_cast<MainWidget*>(widget));
    if (index == -1)
        return;
    mUI.mainTab->setCurrentIndex(index);
}

void MainWindow::UpdateWindowMenu()
{
    mUI.menuWindow->clear();

    const auto count = mUI.mainTab->count();
    const auto curr  = mUI.mainTab->currentIndex();
    for (int i=0; i<count; ++i)
    {
        const auto& text = mUI.mainTab->tabText(i);
        const auto& icon = mUI.mainTab->tabIcon(i);
        QAction* action  = mUI.menuWindow->addAction(icon, text);
        action->setCheckable(true);
        action->setChecked(i == curr);
        action->setProperty("tab-index", i);
        if (i < 9)
            action->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + i)));

        QObject::connect(action, SIGNAL(triggered()),this, SLOT(actionWindowFocus_triggered()));
    }
    // and this is in the window menu
    mUI.menuWindow->addSeparator();
    mUI.menuWindow->addAction(mUI.actionWindowPopOut);
    mUI.menuWindow->addAction(mUI.actionWindowClose);
    mUI.menuWindow->addAction(mUI.actionWindowNext);
    mUI.menuWindow->addAction(mUI.actionWindowPrev);
    mUI.menuWindow->setEnabled(count != 0);
}

void MainWindow::LoadDemoWorkspace(const QString& which)
{
    QString where = qApp->applicationDirPath();

    LoadWorkspace(app::JoinPath(where, which));
}

bool MainWindow::LoadWorkspace(const QString& dir)
{
    auto workspace = std::make_unique<app::Workspace>();

    gfx::SetResourceLoader(workspace.get());

    if (!workspace->LoadWorkspace(dir))
    {
        return false;
    }

    const auto& settings = workspace->GetProjectSettings();
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

    format.setSamples(settings.multisample_sample_count);
    QSurfaceFormat::setDefaultFormat(format);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);

    // desktop dimensions
    const QList<QScreen*>& screens = QGuiApplication::screens();
    const QScreen* screen0 = screens[0];
    const QSize& size = screen0->availableVirtualSize();

    // block main tab signals
    QSignalBlocker blocker(mUI.mainTab);

    // Load workspace windows and their content.
    bool success = true;

    unsigned show_resource_bits = ~0u;
    QStringList session;
    workspace->GetUserProperty("session_files", &session);
    workspace->GetUserProperty("show_resource_bits", &show_resource_bits);
    for (const auto& file : session)
    {
        Settings settings(file);
        if (!settings.Load())
        {
            WARN("Failed to load session settings file '%1'.", file);
            success = false;
            continue;
        }
        const auto& klass = settings.getValue("MainWindow", "class_name", QString(""));
        const auto& id    = settings.getValue("MainWindow", "widget_id", QString(""));
        MainWidget* widget = nullptr;
        if (klass == MaterialWidget::staticMetaObject.className())
            widget = new MaterialWidget(workspace.get());
        else if (klass == ParticleEditorWidget::staticMetaObject.className())
            widget = new ParticleEditorWidget(workspace.get());
        else if (klass == ShapeWidget::staticMetaObject.className())
            widget = new ShapeWidget(workspace.get());
        else if (klass == AnimationTrackWidget::staticMetaObject.className())
            widget = new AnimationTrackWidget(workspace.get());
        else if (klass == EntityWidget::staticMetaObject.className())
            widget = new EntityWidget(workspace.get());
        else if (klass == SceneWidget::staticMetaObject.className())
            widget = new SceneWidget(workspace.get());
        else if (klass == ScriptWidget::staticMetaObject.className())
            widget = new ScriptWidget(workspace.get());
        else if (klass == UIWidget::staticMetaObject.className())
            widget = new UIWidget(workspace.get());
        else BUG("Unhandled widget type.");
        widget->SetId(id);
        if (!widget->LoadState(settings))
        {
            WARN("Widget '%1 failed to load state.", widget->windowTitle());
            success = false;
        }
        const bool has_own_window = settings.getValue("MainWindow", "has_own_window", false);
        if (has_own_window)
        {
            ChildWindow* window = ShowWidget(widget , true);
            const auto xpos  = settings.getValue("MainWindow", "window_xpos", window->x());
            const auto ypos  = settings.getValue("MainWindow", "window_ypos", window->y());
            const auto width = settings.getValue("MainWindow", "window_width", window->width());
            const auto height = settings.getValue("MainWindow", "window_height", window->height());
            if (xpos < size.width() && ypos < size.height())
                window->move(xpos, ypos);

            window->resize(width, height);
        }
        else
        {
            ShowWidget(widget, false);
        }
        // remove the file, no longer needed.
        QFile::remove(file);
        DEBUG("Loaded widget '%1'", widget->windowTitle());
    }

    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(workspace->GetName()));
    SetValue(mUI.grpHelp, workspace->GetName());
    mUI.workspace->setModel(&mWorkspaceProxy);
    mUI.actionSaveWorkspace->setEnabled(true);
    mUI.actionCloseWorkspace->setEnabled(true);
    mUI.actionSelectResourceForEditing->setEnabled(true);
    mUI.menuWorkspace->setEnabled(true);
    mWorkspace = std::move(workspace);
    mWorkspaceProxy.SetModel(mWorkspace.get());
    mWorkspaceProxy.setSourceModel(mWorkspace->GetResourceModel());
    mWorkspaceProxy.SetShowBits(show_resource_bits);
    mWorkspaceProxy.invalidate();

    const auto current_index = mWorkspace->GetUserProperty("focused_widget_index", 0);
    if (current_index < GetCount(mUI.mainTab))
    {
        mUI.mainTab->setCurrentIndex(current_index);
        on_mainTab_currentChanged(current_index);
    }
    else
    {
        on_mainTab_currentChanged(-1);
    }
    return success;
}

bool MainWindow::SaveWorkspace()
{
    // if no workspace, the nothing to do.
    if (!mWorkspace)
        return true;

    bool success = true;

    // session files list, stores the list of temp files
    // generated for each currently open widget
    QStringList session_file_list;

    // for each widget that is currently open in the maintab
    // we generate a temporary json file in which we save the UI state
    // of that widget. When the application is relaunched we use the
    // data in the JSON file to recover the widget and it's contents.
    const auto tabs = mUI.mainTab->count();
    for (int i=0; i<tabs; ++i)
    {
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppFilePath("temp");
        const auto& file = app::GetAppFilePath("temp/" + temp + ".json");
        QDir dir;
        if (!dir.mkpath(path))
        {
            ERROR("Failed to create folder: '%1'", path);
            success = false;
            continue;
        }
        const auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));

        Settings settings(file);
        settings.setValue("MainWindow", "class_name", widget->metaObject()->className());
        settings.setValue("MainWindow", "widget_id", widget->GetId());
        if (!widget->SaveState(settings))
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save widget '%1' settings.",  widget->windowTitle());
            success = false;
            continue;
        }
        session_file_list << file;
        DEBUG("Saved widget '%1'", widget->windowTitle());
    }

    // for each widget that is contained inside a window (instead of being in the main tab)
    // we (also) generate a temporary JSON file in which we save the widget's UI state.
    // When the application is relaunched we use the data in the JSON to recover
    // the widget and it's contents and also to recreate a new containing ChildWindow
    // with same dimensions and desktop position.
    for (const auto* child : mChildWindows)
    {
        const auto& temp = app::RandomString();
        const auto& path = app::GetAppFilePath("temp");
        const auto& file = app::GetAppFilePath("temp/" + temp + ".json");
        QDir dir;
        if (!dir.mkpath(path))
        {
            ERROR("Failed to create folder: '%1'", path);
            success = false;
            continue;
        }
        const MainWidget* widget = child->GetWidget();

        Settings settings(file);
        settings.setValue("MainWindow", "class_name", widget->metaObject()->className());
        settings.setValue("MainWindow", "has_own_window", true);
        settings.setValue("MainWindow", "window_xpos", child->x());
        settings.setValue("MainWindow", "window_ypos", child->y());
        settings.setValue("MainWindow", "window_width", child->width());
        settings.setValue("MainWindow", "window_height", child->height());
        if (!widget->SaveState(settings))
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
            continue;
        }
        if (!settings.Save())
        {
            ERROR("Failed to save widget '%1' settings.", widget->windowTitle());
            success = false;
        }
        session_file_list << file;
        DEBUG("Saved widget '%1'", widget->windowTitle());
    }
    mWorkspace->SetUserProperty("show_resource_bits", mWorkspaceProxy.GetShowBits());
    mWorkspace->SetUserProperty("session_files", session_file_list);
    if (mCurrentWidget)
    {
        const auto index_of = mUI.mainTab->indexOf(mCurrentWidget);
        mWorkspace->SetUserProperty("focused_widget_index", index_of);
    }
    if (mPlayWindow)
    {
        mPlayWindow->SaveState();
    }

    if (!mWorkspace->SaveWorkspace())
        return false;

    return true;
}

void MainWindow::CloseWorkspace()
{
    if (!mWorkspace)
    {
        ASSERT(mChildWindows.empty());
        ASSERT(mUI.mainTab->count() == 0);
        ASSERT(!mPlayWindow.get());
        return;
    }

    // note that here we don't care about saving any state.
    // this is only for closing everything, closing the tabs
    // and the child windows if any are open.

    // make sure we're not getting nasty unwanted recursion
    QSignalBlocker blocker(mUI.mainTab);

    // delete widget objects in the main tab.
    while (mUI.mainTab->count())
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(0));
        widget->Shutdown();
        //               !!!!! WARNING !!!!!
        // setParent(nullptr) will cause an OpenGL memory leak
        //
        // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
        // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
        // https://bugreports.qt.io/browse/QTBUG-69429
        //
        // widget->setParent(nullptr);

        // cleverly enough this will remove the tab. so the loop
        // here must be carefully done to access the tab at index 0
        delete widget;
    }
    mUI.mainTab->clear();

    // delete child windows
    for (auto* child : mChildWindows)
    {
        child->Shutdown();
        child->close();
        delete child;
    }
    mChildWindows.clear();

    mCurrentWidget = nullptr;

    if (mGameProcess.IsRunning())
    {
        mGameProcess.Kill();
    }

    if (mIPCHost)
    {
        mIPCHost->Close();
        mIPCHost.reset();
    }

    if (mPlayWindow)
    {
        mPlayWindow->Shutdown();
        mPlayWindow->close();
        mPlayWindow.reset();
    }

    // update window menu.
    UpdateWindowMenu();

    mUI.actionSaveWorkspace->setEnabled(false);
    mUI.actionCloseWorkspace->setEnabled(false);
    mUI.actionSelectResourceForEditing->setEnabled(false);
    mUI.menuWorkspace->setEnabled(false);
    mUI.menuEdit->setEnabled(false);
    mUI.menuTemp->setEnabled(false);
    mUI.workspace->setModel(nullptr);
    mWorkspaceProxy.SetModel(nullptr);
    mWorkspaceProxy.setSourceModel(nullptr);

    setWindowTitle(QString("%1").arg(APP_TITLE));

    mWorkspace.reset();

    gfx::SetResourceLoader(nullptr);

    ShowHelpWidget();
}

void MainWindow::showWindow()
{
    show();
}

void MainWindow::iterateGameLoop()
{
    if (!mWorkspace)
        return;

    const auto elapsed_since = ElapsedSeconds();
    const auto& settings = mWorkspace->GetProjectSettings();
    const auto time_step = 1.0 / settings.updates_per_second;

    mTimeAccum += elapsed_since;

    while (mTimeAccum >= time_step)
    {
        if (mCurrentWidget)
        {
            mCurrentWidget->Update(time_step);
        }
        for (auto* child : mChildWindows)
        {
            child->Update(time_step);
        }
        mTimeTotal += time_step;
        mTimeAccum -= time_step;
    }

    GfxWindow::BeginFrame();

    // render all widgets
    if (mCurrentWidget)
    {
        mCurrentWidget->Render();
    }
    for (auto* child : mChildWindows)
    {
        child->Render();
    }

    if (mPlayWindow)
    {
        // there's small semantic change here between the other
        // main loop implementations and calling App::BeginMainLoop
        // in the sense that other implementations call BeginMainLoop
        // first and then dispatch window system events, call, tick
        // update, render and EndMainLoop.
        // however this is the only place where this can be called
        // since modal dialogs will cause Qt to enter a temporary
        // event loop that dispatches the window events which means
        // that we have no other chance to call BeginMainLoop other than this.
        // Hopefully this will not become an issue but might be something
        // to keep in mind!
        mPlayWindow->BeginMainLoop();
        // run the playwindow main loop once.
        mPlayWindow->RunOnce();
        // See the comments up top about BeginMainLoop, the
        // same comment applies to EndMainLoop as well,
        // i.e. window events may be dispatched outside these
        // two functions being called.
        mPlayWindow->EndMainLoop();
    }

    // show stats for the current tab if any
    if (mCurrentWidget)
    {
        MainWidget::Stats stats;
        if (mCurrentWidget->GetStats(&stats))
        {
            SetValue(mUI.statTime, QString::number(stats.time));
            SetValue(mUI.statFps,  QString::number((int)stats.fps));
            SetValue(mUI.statVsync, stats.vsync ? QString("ON") : QString("OFF"));
        }
    }

    GfxWindow::EndFrame();
    GfxWindow::CleanGarbage();
}

bool MainWindow::haveAcceleratedWindows() const
{
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        if (widget->IsAccelerated())
            return true;
    }
    for (const auto* child : mChildWindows)
    {
        if (!child->IsClosed() && child->IsAccelerated())
            return true;
    }
    if (mPlayWindow)
        return true;

    return false;
}

void MainWindow::on_menuEdit_aboutToShow()
{
    DEBUG("Edit menu about to show.");

    if (!mCurrentWidget)
    {
        mUI.actionCut->setEnabled(false);
        mUI.actionCopy->setEnabled(false);
        mUI.actionPaste->setEnabled(false);
        mUI.actionUndo->setEnabled(false);
        return;
    }
    // Paste won't work correctly when invoked through the menu.
    // The problem is that we're using QWindow inside GfxWidget and that
    // means that when the menu opens the widget loses keyboard focus.
    // If the widget checks inside MainWidget::Paste whether the GfxWidget actually
    // is focused or not this won't work correctly. And it kinda needs to do this
    // inorder to implement the paste only when the window is actually in focus.
    // As in it'd be weird if something was pasted into the gfx widget while some
    // other widget actually had the focus.

    // instead of having some complicated signal system on window activation to indicate
    // whether something can be copy/pasted we enable/disable these on last minute
    // menu activation. Then if the user wants to invoke the actions throuhg the keyboard
    // shortcuts the widget implementations will actually need to check for the
    // right state before implementing the action.
    mUI.actionCut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanCut, &mClipboard));
    mUI.actionCopy->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanCopy, &mClipboard));
    mUI.actionPaste->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanPaste, &mClipboard));
    mUI.actionUndo->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanUndo));
}

void MainWindow::on_mainTab_currentChanged(int index)
{
    DEBUG("Main tab current changed %1", index);

    if (mCurrentWidget)
    {
        mCurrentWidget->Deactivate();
        mUI.statusBarFrame->setVisible(false);
    }

    mCurrentWidget = nullptr;
    mUI.mainToolBar->clear();
    mUI.menuTemp->clear();

    SetValue(mUI.statTime, QString(""));
    SetValue(mUI.statFps,  QString(""));
    SetValue(mUI.statVsync, QString(""));

    if (index != -1)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));

        widget->Activate();
        widget->AddActions(*mUI.mainToolBar);
        widget->AddActions(*mUI.menuTemp);

        QString name = widget->metaObject()->className();
        name.remove("gui::");
        name.remove("Widget");
        mUI.menuEdit->setEnabled(true);
        mUI.menuTemp->setEnabled(true);
        mUI.menuTemp->setTitle(name);
        mCurrentWidget = widget;
        mUI.actionZoomIn->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomIn));
        mUI.actionZoomOut->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanZoomOut));
        mUI.actionReloadShaders->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadShaders));
        mUI.actionReloadTextures->setEnabled(widget->CanTakeAction(MainWidget::Actions::CanReloadTextures));
        mUI.statusBarFrame->setVisible(widget->IsAccelerated());
    }
    else
    {
        mUI.menuTemp->setEnabled(false);
        mUI.menuEdit->setEnabled(false);
        mUI.actionZoomIn->setEnabled(false);
        mUI.actionZoomOut->setEnabled(false);
    }
    UpdateWindowMenu();
    ShowHelpWidget();
}

void MainWindow::on_mainTab_tabCloseRequested(int index)
{
    auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));
    if (!widget->ConfirmClose())
        return;

    if (widget == mCurrentWidget)
        mCurrentWidget = nullptr;

    // does not delete the widget.
    mUI.mainTab->removeTab(index);

    widget->Shutdown();

    //               !!!!! WARNING !!!!!
    // setParent(nullptr) will cause an OpenGL memory leak
    //
    // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
    // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
    // https://bugreports.qt.io/browse/QTBUG-69429
    //
    //widget->setParent(nullptr);

    delete widget;

    // rebuild window menu.
    UpdateWindowMenu();

}

void MainWindow::on_actionExit_triggered()
{
    this->close();
}

void MainWindow::on_actionHelp_triggered()
{
    QStringList help_args;
    help_args << "-collectionFile";
    help_args << "help/help.qhc";
    QString help_name = "assistant-qt5";
#if defined(WINDOWS_OS)
    help_name.append(".exe");
#endif
    const auto& help = app::JoinPath(QCoreApplication::applicationDirPath(), help_name);
    if (!QProcess::startDetached(help, help_args))
    {
        ERROR("Failed to start assistant-qt5.");
    }
}

void MainWindow::on_actionAbout_triggered()
{
    DlgAbout dlg(this);
    dlg.exec();
}

void MainWindow::on_actionWindowClose_triggered()
{
    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    on_mainTab_tabCloseRequested(cur);
}

void MainWindow::on_actionWindowNext_triggered()
{
    // cycle to next tab in the maintab

    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    const auto size = mUI.mainTab->count();
    const auto next = (cur + 1) % size;
    mUI.mainTab->setCurrentIndex(next);
}

void MainWindow::on_actionWindowPrev_triggered()
{
    // cycle to previous tab in the maintab

    const auto cur = mUI.mainTab->currentIndex();
    if (cur == -1)
        return;

    const auto size = mUI.mainTab->count();
    const auto prev = (cur == 0) ? size - 1 : cur - 1;
    mUI.mainTab->setCurrentIndex(prev);
}

void MainWindow::on_actionWindowPopOut_triggered()
{
    const auto index = mUI.mainTab->currentIndex();
    if (index == -1)
        return;

    auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(index));

    // does not delete the widget. should trigger currentChanged.
    mUI.mainTab->removeTab(index);
    widget->setParent(nullptr);

    ChildWindow* window = ShowWidget(widget, true);
    widget->show();
    widget->updateGeometry();
    window->updateGeometry();
    // seems that we need some delay (presumaly to allow some
    // event processing to take place) on Windows before
    // calling the update geometry. Without this the window is  
    // somewhat fucked up in its appearance. (Layout is off)
    QTimer::singleShot(10, window, &QWidget::updateGeometry);
    QTimer::singleShot(10, widget, &QWidget::updateGeometry);
}

void MainWindow::on_actionCut_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Cut(mClipboard);
}
void MainWindow::on_actionCopy_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Copy(mClipboard);
}
void MainWindow::on_actionPaste_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Paste(mClipboard);
}

void MainWindow::on_actionUndo_triggered()
{
    if (!mCurrentWidget)
        return;

    mCurrentWidget->Undo();
}

void MainWindow::on_actionZoomIn_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ZoomIn();
    mUI.actionZoomIn->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
}
void MainWindow::on_actionZoomOut_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ZoomOut();
    mUI.actionZoomOut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
}

void MainWindow::on_actionReloadShaders_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ReloadShaders();
    INFO("'%1' shaders reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionReloadTextures_triggered()
{
    if (!mCurrentWidget)
        return;
    mCurrentWidget->ReloadTextures();
    INFO("'%1' textures reloaded.", mCurrentWidget->windowTitle());
}

void MainWindow::on_actionNewMaterial_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new MaterialWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewParticleSystem_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new ParticleEditorWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewCustomShape_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new ShapeWidget(mWorkspace.get()), open_new_window);
}
void MainWindow::on_actionNewEntity_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new EntityWidget(mWorkspace.get()), open_new_window);
}
void MainWindow::on_actionNewScene_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new SceneWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewScript_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new ScriptWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionNewUI_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(new UIWidget(mWorkspace.get()), open_new_window);
}

void MainWindow::on_actionImportFiles_triggered()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Files"));
    if (files.isEmpty())
        return;
    ImportFiles(files);
}

void MainWindow::on_actionExportJSON_triggered()
{
    if (!mWorkspace)
        return;
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    if (indices.isEmpty())
        return;
    const auto& filename = QFileDialog::getSaveFileName(this,
        tr("Export Resources as Json"),
        tr("content.json"),
        tr("JSON (*.json)"));
    if (filename.isEmpty())
        return;

    data::JsonObject json;
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(indices[i].row());
        resource.Serialize(json);
    }
    data::JsonFile file;
    file.SetRootObject(json);
    const auto [ok, error] = file.Save(app::ToUtf8(filename));
    if (!ok)
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to export the JSON into file.\n"
                       "Please see the log for details."));
        msg.exec();
    }
    INFO("Exported %1 resource(s) into '%2'", indices.size(), filename);
    NOTE("Exported %1 resource(s) into '%2'", indices.size(), filename);
}

void MainWindow::on_actionEditResource_triggered()
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    EditResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewWindow_triggered()
{
    const auto open_new_window = true;

    EditResources(open_new_window);
}

void MainWindow::on_actionEditResourceNewTab_triggered()
{
    const auto open_new_window = false;

    EditResources(open_new_window);
}


void MainWindow::on_actionDeleteResource_triggered()
{
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setText(tr("Are you sure you want to delete the selected resources ?"));
    if (msg.exec() == QMessageBox::No)
        return;
    QModelIndexList selected = mUI.workspace->selectionModel()->selectedRows();
    mWorkspace->DeleteResources(selected);
}

void MainWindow::on_actionDuplicateResource_triggered()
{
    QModelIndexList selected = mUI.workspace->selectionModel()->selectedRows();
    mWorkspace->DuplicateResources(selected);
}

void MainWindow::on_actionSaveWorkspace_triggered()
{
    if (!mWorkspace->SaveWorkspace())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Workspace saving failed. See the log for more information."));
        msg.exec();
        return;
    }
    NOTE("Workspace saved.");
}

void MainWindow::on_actionLoadWorkspace_triggered()
{
    const auto& file = QFileDialog::getOpenFileName(this, tr("Select Workspace"),
        QString(), QString("workspace.json"));
    if (file.isEmpty())
        return;

    const QFileInfo info(file);
    const QString dir = info.path();

    // check here whether the files actually exist.
    // todo: maybe move into workspace to validate folder
    if (MissingFile(app::JoinPath(dir, "content.json")) ||
        MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        // todo: could ask if the user would like to create a new workspace instead.
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr(            "The folder"
                                 "\n\n'%1'\n\n"
                       "doesn't seem contain workspace files.\n").arg(dir));
        msg.exec();
        return;
    }

    // todo: should/could ask about saving. the current workspace if we have any.

    if (!SaveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    CloseWorkspace();

    // load new workspace.
    if (!LoadWorkspace(dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to open workspace\n"
                    "\n\n'%1'\n\n"
                    "See the application log for more information.").arg(dir));
        msg.exec();
        return;
    }

    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(mWorkspace->GetName()));

    if (!mRecentWorkspaces.contains(dir))
        mRecentWorkspaces.insert(0, dir);
    if (mRecentWorkspaces.size() > 10)
        mRecentWorkspaces.pop_back();

    BuildRecentWorkspacesMenu();
    ShowHelpWidget();
    NOTE("Loaded workspace.");
}

void MainWindow::on_actionNewWorkspace_triggered()
{
    // Note: it might be tempting in terms of UX to just let the
    // user create a new workspace object and start working adding
    // content, however this has the problem that since we don't know
    // where the workspace would end up being saved we don't know how
    // to map content paths (relative to the workspace without location).
    // (Also it could be that at some point some of the content is
    // copied to some workspace folders.)
    // Therefore we need this clunkier UX where the user must first be
    // prompted for the location of the workspace before it can be
    // used to create content.

    // todo: might want to improve the dialog here to be a custom dialog
    // with an option to create some directory for the new workspace
    const auto& workspace_dst_dir = QFileDialog::getExistingDirectory(this,tr("Select New Workspace Directory"));
    if (workspace_dst_dir.isEmpty())
        return;

    // todo: should/could ask about saving. the current workspace if we have any.
    if (!SaveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    CloseWorkspace();

    if (!MissingFile(app::JoinPath(workspace_dst_dir, "content.json")) ||
        !MissingFile(app::JoinPath(workspace_dst_dir, "workspace.json")))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("The selected folder seems to already contain a workspace.\n"
            "Are you sure you want to overwrite this ?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    auto workspace = std::make_unique<app::Workspace>();
    if (!workspace->MakeWorkspace(workspace_dst_dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("There was a problem creating the new workspace.\n"
            "Please see the log for details."));
        msg.exec();
        return;
    }

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setText(tr("Would you like to initialize the new workspace with some starter content?"));
    if (msg.exec() == QMessageBox::Yes)
    {
        const QString& starter_src_dir = app::JoinPath(QApplication::applicationDirPath(), "starter");
        app::Workspace starter;
        if (!starter.LoadWorkspace(starter_src_dir))
        {
            QMessageBox msg(this);
            msg.setIcon(QMessageBox::Critical);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setText(tr("Failed to load starter content.\nPlease see the log for details."));
            msg.exec();
            return;
        }

        // copy the data files.
        app::CopyRecursively(app::JoinPath(starter_src_dir, "textures"),
                             app::JoinPath(workspace_dst_dir, "textures"));
        app::CopyRecursively(app::JoinPath(starter_src_dir, "lua"),
                             app::JoinPath(workspace_dst_dir, "lua"));
        // copy the resources. todo: maybe just copy the content.json and workspace.json?
        for (size_t i=0; i<starter.GetNumUserDefinedResources(); ++i)
        {
            const auto& resource = starter.GetUserDefinedResource(i);
            workspace->SaveResource(resource);
        }
    }

    mUI.workspace->setModel(&mWorkspaceProxy);
    mUI.actionSaveWorkspace->setEnabled(true);
    mUI.actionCloseWorkspace->setEnabled(true);
    mUI.actionSelectResourceForEditing->setEnabled(true);
    mUI.menuWorkspace->setEnabled(true);
    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(workspace->GetName()));
    mWorkspace = std::move(workspace);
    mWorkspaceProxy.SetModel(mWorkspace.get());
    mWorkspaceProxy.setSourceModel(mWorkspace->GetResourceModel());
    gfx::SetResourceLoader(mWorkspace.get());

    ShowHelpWidget();
    NOTE("New workspace created.");
}

void MainWindow::on_actionCloseWorkspace_triggered()
{
    // todo: should/could ask about saving. the current workspace if we have any.
    if (!SaveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
            "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    CloseWorkspace();
}

void MainWindow::on_actionSettings_triggered()
{
    const QString current_style = mSettings.style_name;
    TextEditor::Settings editor_settings;
    TextEditor::GetDefaultSettings(&editor_settings);

    DlgSettings dlg(this, mSettings, editor_settings);
    if (dlg.exec() == QDialog::Rejected)
        return;

    TextEditor::SetDefaultSettings(editor_settings);

    if (current_style == mSettings.style_name)
        return;

    QStyle* style = QApplication::setStyle(mSettings.style_name);
    if (style == nullptr) {
        WARN("No such application style '%1'", mSettings.style_name);
    } else {
        mApplication.setStyleSheet("");
        QApplication::setPalette(style->standardPalette());
        DEBUG("Application style set to '%1'", mSettings.style_name);
    }
}

void MainWindow::on_actionImagePacker_triggered()
{
    DlgImgPack dlg(this);
    dlg.exec();
}
void MainWindow::on_actionClearLog_triggered()
{
    app::EventLog::get().clear();
}

void MainWindow::on_actionLogShowInfo_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Info, val);
    mEventLog.invalidate();
}
void MainWindow::on_actionLogShowWarning_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Warning, val);
    mEventLog.invalidate();
}
void MainWindow::on_actionLogShowError_toggled(bool val)
{
    mEventLog.SetVisible(app::EventLogProxy::Show::Error, val);
    mEventLog.invalidate();
}

void MainWindow::actionWindowFocus_triggered()
{
    // this signal comes from an action in the
    // window menu. the index is the index of the widget
    // in the main tab. the menu is rebuilt
    // when the maintab configuration changes.

    auto* action = static_cast<QAction*>(sender());
    const auto tab_index = action->property("tab-index").toInt();

    action->setChecked(true);

    mUI.mainTab->setCurrentIndex(tab_index);
}

void MainWindow::on_eventlist_customContextMenuRequested(QPoint point)
{
    QMenu menu(this);
    mUI.actionClearLog->setEnabled(!app::EventLog::get().isEmpty());
    menu.addAction(mUI.actionClearLog);
    menu.addSeparator();
    menu.addAction(mUI.actionLogShowInfo);
    menu.addAction(mUI.actionLogShowWarning);
    menu.addAction(mUI.actionLogShowError);
    menu.exec(QCursor::pos());
}

void MainWindow::on_workspace_customContextMenuRequested(QPoint)
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    mUI.actionDeleteResource->setEnabled(!indices.isEmpty());
    mUI.actionDuplicateResource->setEnabled(!indices.isEmpty());
    mUI.actionEditResource->setEnabled(!indices.isEmpty());
    mUI.actionEditResourceNewWindow->setEnabled(!indices.isEmpty());
    mUI.actionEditResourceNewTab->setEnabled(!indices.isEmpty());
    mUI.actionExportJSON->setEnabled(!indices.isEmpty());

    // disable edit actions if a non-native resources have been
    // selected. these need to be opened through an external editor.
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(indices[i].row());
        if (resource.IsAudioFile() || resource.IsDataFile()) {
            mUI.actionEditResource->setEnabled(false);
            mUI.actionEditResourceNewTab->setEnabled(false);
            mUI.actionEditResourceNewWindow->setEnabled(false);
        }
    }

    QMenu show;
    show.setTitle("Show ...");
    for (const auto val : magic_enum::enum_values<app::Resource::Type>())
    {
        // skip drawable it's a superclass type and not directly relevant
        // to the user.
        if (val == app::Resource::Type::Drawable)
            continue;

        const std::string name(magic_enum::enum_name(val));
        QAction* action = show.addAction(app::FromUtf8(name));
        connect(action, &QAction::toggled, this, &MainWindow::ToggleShowResource);
        action->setData(magic_enum::enum_integer(val));
        action->setCheckable(true);
        action->setChecked(mWorkspaceProxy.IsShow(val));
        action->setIcon(app::Resource::GetIcon(val));
    }

    QMenu menu(this);
    menu.addAction(mUI.actionNewMaterial);
    menu.addAction(mUI.actionNewParticleSystem);
    menu.addAction(mUI.actionNewCustomShape);
    menu.addAction(mUI.actionNewEntity);
    menu.addAction(mUI.actionNewScene);
    menu.addAction(mUI.actionNewScript);
    menu.addAction(mUI.actionNewUI);
    menu.addSeparator();
    menu.addAction(mUI.actionImportFiles);
    menu.addSeparator();
    menu.addAction(mUI.actionEditResource);
    menu.addAction(mUI.actionEditResourceNewWindow);
    menu.addAction(mUI.actionEditResourceNewTab);
    menu.addSeparator();
    menu.addAction(mUI.actionDuplicateResource);
    menu.addAction(mUI.actionDeleteResource);
    menu.addAction(mUI.actionExportJSON);
    menu.addSeparator();
    menu.addMenu(&show);
    menu.exec(QCursor::pos());
}

void MainWindow::on_workspace_doubleClicked()
{
    on_actionEditResource_triggered();
}

void MainWindow::on_actionPackageResources_triggered()
{
    DlgPackage dlg(QApplication::activeWindow(), *mWorkspace);
    dlg.exec();
}

void MainWindow::on_actionSelectResourceForEditing_triggered()
{
    if (!mWorkspace)
        return;

    DlgOpen dlg(QApplication::activeWindow(), *mWorkspace);
    if (dlg.exec() == QDialog::Rejected)
        return;

    app::Resource* resource = dlg.GetSelected();
    if (resource == nullptr)
        return;
    else if (resource->IsDataFile() || resource->IsAudioFile())
    {
        WARN("Can't edit '%1' since it's not a %2 resource.", resource->GetName(), APP_TITLE);
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setText(tr("Can't edit '%1' since it's not a %2 resource.").arg(resource->GetName(), APP_TITLE));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
        return;
    }
    if (!FocusWidget(resource->GetId()))
    {
        const auto new_window = mSettings.default_open_win_or_tab == "Window";
        ShowWidget(MakeWidget(resource->GetType(), mWorkspace.get(), resource), new_window);
    }
}

void MainWindow::on_actionNewResource_triggered()
{
    if (!mWorkspace)
        return;

    DlgNew dlg(QApplication::activeWindow());
    if (dlg.exec() == QDialog::Rejected)
        return;

    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(dlg.GetType(), mWorkspace.get()), new_window);
}

void MainWindow::on_actionProjectSettings_triggered()
{
    if (!mWorkspace)
        return;

    auto settings = mWorkspace->GetProjectSettings();

    DlgProject dlg(QApplication::activeWindow(), *mWorkspace, settings);
    if (dlg.exec() == QDialog::Rejected)
        return;

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(settings.multisample_sample_count);
    QSurfaceFormat::setDefaultFormat(format);

    mWorkspace->SetProjectSettings(settings);

    GfxWindow::SetDefaultFilter(settings.default_min_filter);
    GfxWindow::SetDefaultFilter(settings.default_mag_filter);
}

void MainWindow::on_actionProjectPlay_triggered()
{
    if (!mWorkspace)
        return;
    else if (mPlayWindow)
        return;
    else if (mGameProcess.IsRunning())
        return;

    const auto& settings = mWorkspace->GetProjectSettings();
    if (settings.GetApplicationLibrary().isEmpty())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't set the application library for the project.\n"
                    "The game should be built into a library (a .dll or .so file).\n"
                    "You can change the application library in the workspace settings.");
        msg.exec();
        return;
    }

    std::vector<MainWidget*> unsaved;
    for (int i=0; i<mUI.mainTab->count(); ++i)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        if (widget->HasUnsavedChanges())
        {
            if (mSettings.save_automatically_on_play)
                widget->Save();
            else unsaved.push_back(widget);
        }
    }
    for (auto* wnd : mChildWindows)
    {
        auto* widget = wnd->GetWidget();
        if (widget->HasUnsavedChanges())
        {
            if (mSettings.save_automatically_on_play)
                widget->Save();
            else unsaved.push_back(widget);
        }
    }

    if (!unsaved.empty())
    {
        DlgSave dlg(this, unsaved);
        if (dlg.exec() == QDialog::Rejected)
            return;
        const bool save_automatically = dlg.SaveAutomatically();
        mSettings.save_automatically_on_play = save_automatically;
    }


    if (settings.use_gamehost_process)
    {
        // todo: maybe save to some temporary location ?
        // Save workspace for the loading the initial content
        // in the game host
        if (!mWorkspace->SaveWorkspace())
            return;

        ASSERT(!mIPCHost);
        app::IPCHost::Cleanup("gamestudio-local-socket");
        auto ipc = std::make_unique<app::IPCHost>();
        if (!ipc->Open("gamestudio-local-socket"))
            return;

        connect(mWorkspace.get(), &app::Workspace::ResourceUpdated, ipc.get(),
                &app::IPCHost::ResourceUpdated);
        connect(ipc.get(), &app::IPCHost::UserPropertyUpdated, mWorkspace.get(),
                &app::Workspace::UpdateUserProperty);
        DEBUG("Local socket is open.");

        QStringList game_host_args;
        game_host_args << "--no-term-colors";
        game_host_args << "--workspace";
        game_host_args << mWorkspace->GetDir();
        game_host_args << "--app-style";
        game_host_args << mSettings.style_name;

        QString game_host_name = "EditorGameHost";
#if defined(WINDOWS_OS)
        game_host_name.append(".exe");
#endif
        const auto& game_host_file = app::JoinPath(QCoreApplication::applicationDirPath(), game_host_name);
        const auto& game_host_log  = app::JoinPath(QCoreApplication::applicationDirPath(), "game_host.log");
        const auto& game_host_cwd  = QDir::currentPath();
        mGameProcess.EnableTimeout(false);
        mGameProcess.onFinished = [this](){
            DEBUG("Game process finished.");
            if (mGameProcess.GetError() != app::Process::Error::None)
                ERROR("Game process error: '%1'", mGameProcess.GetError());

            mIPCHost->Close();
            mIPCHost.reset();
        };
        mGameProcess.onStdOut = [](const QString& msg) {
            if (msg.isEmpty())
                return;
            // read an encoded log message from the game host process.
            // todo: for the debug message we might want to figure out the
            // source file and line from the message itself by parsing the message.
            // for the time being this is skipped.
            if (msg[0] == "E")
                app::EventLog::get().write(app::Event::Type::Error, msg, "game-host");
            else if (msg[0] == "W")
                app::EventLog::get().write(app::Event::Type::Warning, msg, "game-host");
            else if (msg[0] == "I")
                app::EventLog::get().write(app::Event::Type::Info, msg, "game-host");
            else base::WriteLog(base::LogEvent::Debug, "game-host", 0, app::ToUtf8(msg));
        };
        mGameProcess.onStdErr = [](const QString& msg) {
            app::EventLog::get().write(app::Event::Type::Error, msg, "game-host");
        };
        mGameProcess.Start(game_host_file, game_host_args, game_host_log, game_host_cwd);
        mIPCHost = std::move(ipc);

    }
    else
    {
        if (!mPlayWindow)
        {
            auto window = std::make_unique<PlayWindow>(*mWorkspace);
            window->LoadState();
            window->show();
            if (!window->LoadGame())
                return;
            mPlayWindow = std::move(window);
            emit newAcceleratedWindowOpen();
            QCoreApplication::postEvent(this, new IterateGameLoopEvent);
        }
        else
        {
            // bring to the top of the window stack.
            mPlayWindow->raise();
        }
    }
}

void MainWindow::on_btnBandit_clicked()
{
    LoadDemoWorkspace("demos/bandit");
}

void MainWindow::on_btnBlast_clicked()
{
    LoadDemoWorkspace("demos/blast");
}

void MainWindow::on_btnMaterial_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::Material, mWorkspace.get()), new_window);
}
void MainWindow::on_btnParticle_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::ParticleSystem, mWorkspace.get()), new_window);
}
void MainWindow::on_btnShape_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::Shape, mWorkspace.get()), new_window);
}
void MainWindow::on_btnEntity_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::Entity, mWorkspace.get()), new_window);
}
void MainWindow::on_btnScene_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::Scene, mWorkspace.get()), new_window);
}
void MainWindow::on_btnScript_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::Script, mWorkspace.get()), new_window);
}
void MainWindow::on_btnUI_clicked()
{
    const auto new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(MakeWidget(app::Resource::Type::UI, mWorkspace.get()), new_window);
}

void MainWindow::RefreshUI()
{
    if (mPlayWindow)
    {
        if (mPlayWindow->IsClosed())
        {
            mPlayWindow->SaveState();
            mPlayWindow->Shutdown();
            mPlayWindow->close();
            mPlayWindow.reset();
        }
    }

    // refresh the UI state, and update the tab widget icon/text
    // if needed.
    for (int i=0; i<GetCount(mUI.mainTab);)
    {
        auto* widget = static_cast<MainWidget*>(mUI.mainTab->widget(i));
        widget->Refresh();
        const auto& icon = widget->windowIcon();
        const auto& text = widget->windowTitle();
        mUI.mainTab->setTabText(i, text);
        mUI.mainTab->setTabIcon(i, icon);
        if (widget->ShouldClose())
        {
            // does not delete the widget.
            mUI.mainTab->removeTab(i);
            // shut the widget down, release graphics resources etc.
            widget->Shutdown();
            //               !!!!! WARNING !!!!!
            // setParent(nullptr) will cause an OpenGL memory leak
            //
            // https://forum.qt.io/topic/92179/xorg-vram-leak-because-of-qt-opengl-application/12
            // https://community.khronos.org/t/xorg-vram-leak-because-of-qt-opengl-application/76910/2
            // https://bugreports.qt.io/browse/QTBUG-69429
            //
            delete widget;
        }
        else
        {
            ++i;
        }
    }

    // cull child windows that have been closed.
    // note that we do it this way to avoid having problems with callbacks and recursions.
    for (size_t i=0; i<mChildWindows.size(); )
    {
        ChildWindow* child = mChildWindows[i];
        if (child->ShouldPopIn())
        {
            MainWidget* widget = child->TakeWidget();
            child->close();
            // careful about not fucking up the iteration of this loop
            // however we're going to add as a tab so the widget will
            // go into the main tab not into mChildWindows.
            ShowWidget(widget, false /* new window */);
            // seems that we need some delay (presumably to allow some
            // event processing to take place) on Windows before
            // calling the update geometry. Without this the window is  
            // somewhat fucked up in its appearance. (Layout is off)
            QTimer::singleShot(10, widget, &QWidget::updateGeometry);
            QTimer::singleShot(10, mUI.mainTab, &QWidget::updateGeometry);
        }

        if (child->IsClosed() || child->ShouldPopIn())
        {
            const auto last = mChildWindows.size() - 1;
            std::swap(mChildWindows[i], mChildWindows[last]);
            mChildWindows.pop_back();
            child->close();
            delete child;
        }
        else
        {
            ++i;
        }
    }

    // refresh the child windows
    for (auto* child : mChildWindows)
    {
        child->RefreshUI();
    }

    if (mWorkspace)
        mWorkspace->Tick();

    if (mPlayWindow)
        mPlayWindow->NonGameTick();

    if (mCurrentWidget)
    {
        mUI.actionZoomIn->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomIn));
        mUI.actionZoomOut->setEnabled(mCurrentWidget->CanTakeAction(MainWidget::Actions::CanZoomOut));
    }
}

void MainWindow::ShowNote(const app::Event& event)
{
    if (event.type == app::Event::Type::Note)
    {
        mUI.statusbar->showMessage(event.message, 5000);
        for (const auto* child : mChildWindows)
        {
            child->ShowNote(event.message);
        }
    }
}

void MainWindow::OpenExternalImage(const QString& file)
{
    if (mSettings.image_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for opening images.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.image_editor_executable.isEmpty())
        {
            ERROR("No image editor has been configured.");
            return;
        }
    }
    if (MissingFile(file))
    {
        WARN("Could not find '%1'", file);
    }

    QStringList args;
    QStringList list = mSettings.image_editor_arguments.split(" ", QString::SkipEmptyParts);
    for (const auto& item : list)
    {
        if (item == "${file}")
            args << QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
        else args << item;
    }
    if (!QProcess::startDetached(mSettings.image_editor_executable, args))
    {
        ERROR("Failed to start application '%1'", mSettings.image_editor_executable);

    }
    DEBUG("Start application '%1'", mSettings.image_editor_executable);
}

void MainWindow::OpenExternalShader(const QString& file)
{
    if (mSettings.shader_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for shader files.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.shader_editor_executable.isEmpty())
        {
            ERROR("No shader editor has been configured.");
            return;
        }
    }
    if (MissingFile(file))
    {
        WARN("Could not find '%1'", file);
    }

    QStringList args;
    QStringList list = mSettings.shader_editor_arguments.split(" ", QString::SkipEmptyParts);
    for (const auto& item : list)
    {
        if (item == "${file}")
            args << QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
        else args << item;
    }
    if (!QProcess::startDetached(mSettings.shader_editor_executable, args))
    {
        ERROR("Failed to start application '%1'", mSettings.shader_editor_executable);

    }
    DEBUG("Start application '%1'", mSettings.shader_editor_executable);
}

void MainWindow::OpenExternalScript(const QString& file)
{
    if (mSettings.script_editor_executable.isEmpty())
    {
        QMessageBox msg;
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Question);
        msg.setText("You haven't configured any external application for script files.\n"
                    "Would you like to set one now?");
        if (msg.exec() == QMessageBox::No)
            return;
        on_actionSettings_triggered();
        if (mSettings.script_editor_executable.isEmpty())
        {
            ERROR("No shader editor has been configured.");
            return;
        }
    }
    if (MissingFile(file))
    {
        WARN("Could not find '%1'", file);
    }

    QStringList args;
    QStringList list = mSettings.script_editor_arguments.split(" ", QString::SkipEmptyParts);
    for (const auto& item : list)
    {
        if (item == "${file}")
            args << QDir::toNativeSeparators(mWorkspace->MapFileToFilesystem(file));
        else args << item;
    }
    if (!QProcess::startDetached(mSettings.script_editor_executable, args))
    {
        ERROR("Failed to start application '%1'", mSettings.script_editor_executable);

    }
    DEBUG("Start application '%1'", mSettings.script_editor_executable);
}

void MainWindow::OpenNewWidget(MainWidget* widget)
{
    const auto open_new_window = mSettings.default_open_win_or_tab == "Window";
    ShowWidget(widget, open_new_window);
}

void MainWindow::OpenRecentWorkspace()
{
    const QAction* action = qobject_cast<const QAction*>(sender());

    const QString& dir = action->data().toString();

    // check here whether the files actually exist.
    // todo: maybe move into workspace to validate folder
    if (MissingFile(app::JoinPath(dir, "content.json")) ||
        MissingFile(app::JoinPath(dir, "workspace.json")))
    {
        // todo: could ask if the user would like to create a new workspace instead.
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr(            "The folder"
                                   "\n\n'%1'\n\n"
                                   "doesn't seem contain workspace files.\n"
                                   "Would you like to remove it from the recent workspaces list?").arg(dir));
        if (msg.exec() == QMessageBox::Yes)
        {
            mRecentWorkspaces.removeAll(dir);
            BuildRecentWorkspacesMenu();
        }
        return;
    }

    // todo: should/could ask about saving. the current workspace if we have any changes.
    if (!SaveWorkspace())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText("There was a problem saving the current workspace.\n"
                    "Do you still want to continue ?");
        if (msg.exec() == QMessageBox::No)
            return;
    }
    // close existing workspace if any.
    CloseWorkspace();

    // load new workspace.
    if (!LoadWorkspace(dir))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setText(tr("Failed to open workspace\n"
                       "\n\n'%1'\n\n"
                       "See the application log for more information.").arg(dir));
        msg.exec();
        return;
    }

    setWindowTitle(QString("%1 - %2").arg(APP_TITLE).arg(mWorkspace->GetName()));

    NOTE("Loaded workspace.");
}

void MainWindow::ToggleShowResource()
{
    QAction* action = qobject_cast<QAction*>(sender());

    const auto payload = action->data().toInt();
    const auto type = magic_enum::enum_cast<app::Resource::Type>(payload);
    ASSERT(type.has_value());
    mWorkspaceProxy.SetVisible(type.value(), action->isChecked());
    mWorkspaceProxy.invalidate();
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == IterateGameLoopEvent::GetIdentity())
    {
        iterateGameLoop();

        if (haveAcceleratedWindows())
            QCoreApplication::postEvent(this, new IterateGameLoopEvent);

        return true;
    }
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->ignore();

    // try to perform an orderly shutdown.
    // first save everything and only if that is successful
    // (or the user don't care) we then close the workspace
    // and exit the application.
    if (!SaveWorkspace() || !SaveState())
    {
        QMessageBox msg(this);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setIcon(QMessageBox::Critical);
        msg.setText(tr("There was a problem saving the application state.\r\n"
            "Do you still want to quit the application?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    // close workspace (if any is open)
    CloseWorkspace();

    // accept the event, will quit the application
    event->accept();

    mIsClosed = true;

    emit aboutToClose();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* drag)
{
    if (!mWorkspace)
        return;

    DEBUG("dragEnterEvent");
    drag->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!mWorkspace)
        return;

    DEBUG("dropEvent");

    const auto* mime = event->mimeData();
    if (!mime->hasUrls())
        return;

    QStringList files;

    const auto& urls = mime->urls();
    for (int i=0; i<urls.size(); ++i)
    {
        const auto& name = urls[i].toLocalFile();
        DEBUG("Local file path: %1", name);
        files.append(name);
    }
    ImportFiles(files);
}

void MainWindow::BuildRecentWorkspacesMenu()
{
    mUI.menuRecentWorkspaces->clear();
    for (const auto& recent : mRecentWorkspaces)
    {
        QAction* action = mUI.menuRecentWorkspaces->addAction(QDir::toNativeSeparators(recent));
        action->setData(recent);
        connect(action, &QAction::triggered, this, &MainWindow::OpenRecentWorkspace);
    }
}

bool MainWindow::SaveState()
{
    // persist the properties of the mainwindow itself.
    Settings settings("Ensisoft", APP_TITLE);
    settings.setValue("MainWindow", "log_bits", mEventLog.GetShowBits());
    settings.setValue("MainWindow", "width", width());
    settings.setValue("MainWindow", "height", height());
    settings.setValue("MainWindow", "xpos", x());
    settings.setValue("MainWindow", "ypos", y());
    settings.setValue("MainWindow", "show_toolbar", mUI.mainToolBar->isVisible());
    settings.setValue("MainWindow", "show_statusbar", mUI.statusbar->isVisible());
    settings.setValue("MainWindow", "show_eventlog", mUI.eventlogDock->isVisible());
    settings.setValue("MainWindow", "show_workspace", mUI.workspaceDock->isVisible());
    settings.setValue("Settings", "image_editor_executable", mSettings.image_editor_executable);
    settings.setValue("Settings", "image_editor_arguments", mSettings.image_editor_arguments);
    settings.setValue("Settings", "shader_editor_executable", mSettings.shader_editor_executable);
    settings.setValue("Settings", "shader_editor_arguments", mSettings.shader_editor_arguments);
    settings.setValue("Settings", "default_open_win_or_tab", mSettings.default_open_win_or_tab);
    settings.setValue("Settings", "script_editor_executable", mSettings.script_editor_executable);
    settings.setValue("Settings", "script_editor_arguments", mSettings.script_editor_arguments);
    settings.setValue("Settings", "style_name", mSettings.style_name);
    settings.setValue("Settings", "save_automatically_on_play", mSettings.save_automatically_on_play);
    settings.setValue("MainWindow", "current_workspace",
        (mWorkspace ? mWorkspace->GetDir() : ""));
    settings.setValue("MainWindow", "recent_workspaces", mRecentWorkspaces);

    TextEditor::Settings editor_settings;
    TextEditor::GetDefaultSettings(&editor_settings);
    settings.setValue("TextEditor", "font",                   editor_settings.font_description);
    settings.setValue("TextEditor", "font_size",              editor_settings.font_size);
    settings.setValue("TextEditor", "theme",                  editor_settings.theme);
    settings.setValue("TextEditor", "show_line_numbers",      editor_settings.show_line_numbers);
    settings.setValue("TextEditor", "highlight_syntax",       editor_settings.highlight_syntax);
    settings.setValue("TextEditor", "highlight_current_line", editor_settings.highlight_current_line);
    settings.setValue("TextEditor", "insert_spaces",          editor_settings.insert_spaces);
    settings.setValue("TextEditor", "tab_spaces",             editor_settings.tab_spaces);

    // QMainWindow::SaveState saves the current state of the mainwindow toolbars
    // and dockwidgets.
    settings.setValue("MainWindow", "toolbar_and_dock_state", QMainWindow::saveState());
    return settings.Save();
}

ChildWindow* MainWindow::ShowWidget(MainWidget* widget, bool new_window)
{
    ASSERT(widget->parent() == nullptr);

    // disconnect the signals if already connected. why?
    // because this function is also called when a widget is
    // moved from main tab to a window or vice versa.
    // without disconnect the connections are duped and problems
    // will happen.
    disconnect(widget, &MainWidget::OpenExternalImage,  this, &MainWindow::OpenExternalImage);
    disconnect(widget, &MainWidget::OpenExternalShader, this, &MainWindow::OpenExternalShader);
    disconnect(widget, &MainWidget::OpenExternalScript, this, &MainWindow::OpenExternalScript);
    disconnect(widget, &MainWidget::OpenNewWidget,      this, &MainWindow::OpenNewWidget);
    // connect the important signals here.
    connect(widget, &MainWidget::OpenExternalImage, this, &MainWindow::OpenExternalImage);
    connect(widget, &MainWidget::OpenExternalShader,this, &MainWindow::OpenExternalShader);
    connect(widget, &MainWidget::OpenExternalScript,this, &MainWindow::OpenExternalScript);
    connect(widget, &MainWidget::OpenNewWidget,     this, &MainWindow::OpenNewWidget);

    if (new_window)
    {
        // create a new child window that will hold the widget.
        ChildWindow* child = new ChildWindow(widget, &mClipboard);
        child->SetSharedWorkspaceMenu(mUI.menuWorkspace);

        // resize and relocate on the desktop, by default the window seems
        // to be at a position that requires it to be immediately used and
        // resize by the user. ugh
        const auto width  = std::max((int)(this->width() * 0.8), child->width());
        const auto height = std::max((int)(this->height() *0.8), child->height());
        const auto xpos = x() + (this->width() - width) / 2;
        const auto ypos = y() + (this->height() - height) / 2;
        child->resize(width, height);
        child->move(xpos, ypos);
        // showing the widget *after* resize/move might produce incorrect
        // results since apparently the window's dimensions are not fully
        // know until it has been show (presumably some layout is done)
        // however doing the show first and and then move/resize is visually
        // not very pleasing.
        child->show();

        mChildWindows.push_back(child);
        if (child->IsAccelerated())
        {
            emit newAcceleratedWindowOpen();
            QCoreApplication::postEvent(this, new IterateGameLoopEvent);
        }
        return child;
    }

    // show the widget in the main tab of widgets.
    const auto& text = widget->windowTitle();
    const auto& icon = widget->windowIcon();
    const auto count = mUI.mainTab->count();
    mUI.mainTab->addTab(widget, icon, text);
    mUI.mainTab->setCurrentIndex(count);

    // rebuild window menu and shortcuts
    UpdateWindowMenu();

    if (widget->IsAccelerated())
    {
        emit newAcceleratedWindowOpen();
        QCoreApplication::postEvent(this, new IterateGameLoopEvent);
    }
    // no child window
    return nullptr;
}

void MainWindow::ShowHelpWidget()
{
    // todo: could build the demo setup here dynamically.

    mUI.lblBanditDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/bandit"));
    mUI.lblBlastDir->setText(app::JoinPath(qApp->applicationDirPath(), "demos/blast"));

    if (mWorkspace && mCurrentWidget)
    {
        mUI.helpWidget->setVisible(false);
        mUI.mainTab->setVisible(true);
    }
    else if (mWorkspace && !mCurrentWidget)
    {
        mUI.helpWidget->setVisible(true);
        mUI.helpWidget->setCurrentIndex(0);
        mUI.mainTab->setVisible(false);
        mUI.mainToolBar->clear();
        mUI.mainToolBar->addAction(mUI.actionNewMaterial);
        mUI.mainToolBar->addAction(mUI.actionNewParticleSystem);
        mUI.mainToolBar->addAction(mUI.actionNewCustomShape);
        mUI.mainToolBar->addAction(mUI.actionNewEntity);
        mUI.mainToolBar->addAction(mUI.actionNewScene);
        mUI.mainToolBar->addAction(mUI.actionNewScript);
        mUI.mainToolBar->addAction(mUI.actionNewUI);
        mUI.mainToolBar->addSeparator();
        mUI.mainToolBar->addAction(mUI.actionImportFiles);
    }
    else
    {
        mUI.mainToolBar->clear();
        mUI.mainToolBar->addAction(mUI.actionNewWorkspace);
        mUI.mainToolBar->addAction(mUI.actionLoadWorkspace);
        mUI.helpWidget->setCurrentIndex(1);
        mUI.helpWidget->setVisible(true);
        mUI.mainTab->setVisible(false);
    }
}

void MainWindow::EditResources(bool open_new_window)
{
    const auto& indices = mUI.workspace->selectionModel()->selectedRows();
    for (int i=0; i<indices.size(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(indices[i].row());
        // we don't know how to open these.
        if (resource.GetType() == app::Resource::Type::DataFile ||
            resource.GetType() == app::Resource::Type::AudioFile)
        {
            WARN("Can't edit '%1' since it's not a %2 resource.", resource.GetName(), APP_TITLE);
            continue;
        }

        if (!FocusWidget(resource.GetId()))
            ShowWidget(MakeWidget(resource.GetType() , mWorkspace.get() , &resource) , open_new_window);
    }
}

bool MainWindow::FocusWidget(const QString& id)
{
    for (int i=0; i<GetCount(mUI.mainTab); ++i)
    {
        const auto* widget = static_cast<const MainWidget*>(mUI.mainTab->widget(i));
        if (widget->GetId() == id)
        {
            mUI.mainTab->setCurrentIndex(i);
            return true;
        }
    }
    for (auto* child : mChildWindows)
    {
        const auto* widget = child->GetWidget();
        // if the window is being closed but has not yet been removed
        // the widget can be nullptr, in which case skip the check.
        if (!widget)
            continue;
        if (widget->GetId() == id)
        {
            QTimer::singleShot(10, child, &ChildWindow::ActivateWindow);
            return true;
        }
    }
    return false;
}

void MainWindow::ImportFiles(const QStringList& files)
{
    if (!mWorkspace)
        return;

    mWorkspace->ImportFilesAsResource(files);
}

} // namespace

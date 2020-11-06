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

#define LOGTAG "childwindow"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "base/assert.h"
#include "editor/app/eventlog.h"
#include "editor/gui/childwindow.h"
#include "editor/gui/mainwidget.h"

namespace gui
{

ChildWindow::ChildWindow(MainWidget* widget) : mWidget(widget)
{
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    const auto* klass = mWidget->metaObject()->className();
    DEBUG("Create new child window (widget=%1) '%2'", klass, text);

    mUI.setupUi(this);
    setWindowTitle(text);
    setWindowIcon(icon);

    mUI.verticalLayout->addWidget(widget);
    QString menu_name = klass;
    menu_name.remove("gui::");
    menu_name.remove("Widget");
    mUI.menuTemp->setTitle(menu_name);
    mUI.actionZoomIn->setEnabled(widget->CanZoomIn());
    mUI.actionZoomOut->setEnabled(widget->CanZoomOut());

    mWidget->Activate();
    mWidget->AddActions(*mUI.toolBar);
    mWidget->AddActions(*mUI.menuTemp);
}

ChildWindow::~ChildWindow()
{
    Shutdown();
}

void ChildWindow::RefreshUI()
{
    mWidget->Refresh();
    // update the window icon and title if they have changed.
    const auto& icon = mWidget->windowIcon();
    const auto& text = mWidget->windowTitle();
    setWindowTitle(text);
    setWindowIcon(icon);

    mUI.actionZoomIn->setEnabled(mWidget->CanZoomIn());
    mUI.actionZoomOut->setEnabled(mWidget->CanZoomOut());
}

void ChildWindow::Animate(double secs)
{
    mWidget->Update(secs);
}

void ChildWindow::Render()
{
    mWidget->Render();
}

void ChildWindow::SetSharedWorkspaceMenu(QMenu* menu)
{
    mUI.menubar->insertMenu(mUI.menuEdit->menuAction(), menu);
}

void ChildWindow::Shutdown()
{
    if (mWidget)
    {
        const auto &text = mWidget->windowTitle();
        const auto &klass = mWidget->metaObject()->className();
        DEBUG("Destroy child window (widget=%1) '%2'", klass, text);

        mUI.verticalLayout->removeWidget(mWidget);
        mWidget->setParent(nullptr);

        // make sure we cleanup properly while all the resources
        // such as the OpenGL contexts (and the window) are still
        // valid.
        mWidget->Shutdown();

        delete mWidget;
        mWidget = nullptr;
    }
}

MainWidget* ChildWindow::TakeWidget()
{
    auto* ret = mWidget;
    mUI.verticalLayout->removeWidget(mWidget);
    mWidget->setParent(nullptr);
    mWidget = nullptr;
    return ret;
}

void ChildWindow::on_actionClose_triggered()
{
    // this will create close event
    close();
}
void ChildWindow::on_actionPopIn_triggered()
{
    mPopInRequested = true;
}
void ChildWindow::on_actionReloadShaders_triggered()
{
    mWidget->ReloadShaders();
    INFO("'%1' shaders reloaded.", mWidget->windowTitle());
}

void ChildWindow::on_actionReloadTextures_triggered()
{
    mWidget->ReloadTextures();
    INFO("'%1' textures reloaded.", mWidget->windowTitle());
}

void ChildWindow::on_actionZoomIn_triggered()
{
    mWidget->ZoomIn();
    mUI.actionZoomIn->setEnabled(mWidget->CanZoomIn());
}
void ChildWindow::on_actionZoomOut_triggered()
{
    mWidget->ZoomOut();
    mUI.actionZoomOut->setEnabled(mWidget->CanZoomOut());
}

void ChildWindow::closeEvent(QCloseEvent* event)
{
    event->accept();

    if (!mWidget)
        return;

    if (!mWidget->ConfirmClose())
    {
        event->ignore();
        return;
    }
    const auto& text  = mWidget->windowTitle();
    const auto& klass = mWidget->metaObject()->className();
    DEBUG("Close child window (widget=%1) '%2'", klass, text);
    // we could emit an event here to indicate that the window
    // is getting closed but that's a sure-fire way of getting
    // unwanted recursion that will fuck shit up.  (i.e this window
    // getting deleted which will run the destructor which will
    // make this function have an invalided *this* pointer. bad.
    // so instead of doing that we just set a flag and the
    // mainwindow will check from time to if the window object
    // should be deleted.
    mClosed = true;
}

} // namespace

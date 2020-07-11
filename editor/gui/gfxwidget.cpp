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

#define LOGTAG "gfx"

#include "config.h"

#include "warnpush.h"
#  include <QMenu>
#  include <QContextMenuEvent>
#  include <QMouseEvent>
#  include <QBasicTimer>
#  include <color_dialog.hpp>
#include "warnpop.h"

#include "editor/app/eventlog.h"
#include "graphics/drawing.h"
#include "utility.h"
#include "gfxwidget.h"

namespace gui
{

GfxWindow::~GfxWindow()
{
    ASSERT(!mCustomGraphicsDevice);
    ASSERT(!mCustomGraphicsPainter);
    DEBUG("Destroy GfxWindow");
}

GfxWindow::GfxWindow()
{
    // weird but not able to connect frameSwapped directly to update.
    // (compiler error)
    connect(this, &QOpenGLWindow::frameSwapped,
            this, &GfxWindow::frameSwapped);

    // There's the problem that it seems a bit tricky to get the
    // OpenGLWidget's size (framebuffer size) properly when
    // starting things up. When the widget is loaded  there are
    // multiple invokations of resizeGL where the first call(s)
    // don't report the final size that the widget will eventually
    // have. I suspect this happens because there is some layout
    // engine adjusting the widget sizes. However this is a problem
    // if we want to for example have a user modifyable viewport
    // default to the initial size of the widget.
    // Doing a little hack here and hoping that when the first timer
    // event fires we'd have the final size correctly.
    QTimer::singleShot(10, this, &GfxWindow::doInit);
}

void GfxWindow::dispose()
{
    // Make sure this window's context is current when releasing the graphics
    // resources. If we have multiple GfxWindows each with their own context
    // it's possible that each one might have resources with the same name
    // for example texture 0, however if the wrong context is current
    // then the device will end deleting resources that actually belong
    // to a different device! *OOPS*
    makeCurrent();

    mCustomGraphicsDevice.reset();
    mCustomGraphicsPainter.reset();
    DEBUG("Released GfxWindow device and painter.");
}

void GfxWindow::initializeGL()
{
    // Context implementation based on widget/context provided by Qt.
    class WidgetContext : public gfx::Device::Context
    {
    public:
        WidgetContext(QOpenGLWindow* window) : mWindow(window)
        {}
        virtual void Display() override
        {
            // No need to implement this, Qt will take care of this
        }
        virtual void MakeCurrent() override
        {
            mWindow->makeCurrent();
        }
        virtual void* Resolve(const char* name) override
        {
            return (void*)mWindow->context()->getProcAddress(name);
        }
    private:
        QOpenGLWindow* mWindow = nullptr;
    };

    // create custom painter for fancier shader based effects.
    mCustomGraphicsDevice  = gfx::Device::Create(gfx::Device::Type::OpenGL_ES2,
        std::make_shared<WidgetContext>(this));
    mCustomGraphicsPainter = gfx::Painter::Create(mCustomGraphicsDevice);
}

void GfxWindow::paintGL()
{
    if (!mCustomGraphicsDevice)
        return;

    // Qt should have made this window's context current

    const quint64 ms = mClock.restart();

    mCustomGraphicsDevice->BeginFrame();
    mCustomGraphicsDevice->ClearColor(mClearColor);
    if (onPaintScene)
        onPaintScene(*mCustomGraphicsPainter, ms/1000.0);

    if (mShowFps)
    {
        mNumFrames++;
        mFrameTime += ms;
        if (mFrameTime >= 1000)
        {
            // how many frames did get to display in the last period
            const auto secs = mFrameTime / 1000.0;
            mCurrentFps = mNumFrames / secs;
            mNumFrames  = 0;
            mFrameTime  = 0;
        }
        gfx::DrawTextRect(*mCustomGraphicsPainter,
            base::FormatString("%1 FPS", (unsigned)mCurrentFps),
            "app://fonts/ARCADE.TTF", 28,
            gfx::FRect(10, 20, 150, 100),
            gfx::Color::HotPink,
            gfx::TextAlign::AlignLeft | gfx::TextAlign::AlignTop);
    }
    mCustomGraphicsDevice->EndFrame(true /*display*/);
    mCustomGraphicsDevice->CleanGarbage(60);
}
void GfxWindow::doInit()
{
    const auto w = width();
    const auto h = height();
    onInitScene(w, h);
 }

void GfxWindow::mouseMoveEvent(QMouseEvent* mickey)
{
    onMouseMove(mickey);
}
void GfxWindow::mousePressEvent(QMouseEvent* mickey)
{
    onMousePress(mickey);
}
void GfxWindow::mouseReleaseEvent(QMouseEvent* mickey)
{
    onMouseRelease(mickey);
}

void GfxWindow::keyPressEvent(QKeyEvent* key)
{
    onKeyPress(key);
}

void GfxWindow::wheelEvent(QWheelEvent* wheel)
{
    onMouseWheel(wheel);
}

void GfxWindow::toggleShowFps()
{
    mShowFps = !mShowFps;
    mFrameTime = 0;
    mNumFrames = 0;
}

void GfxWindow::frameSwapped()
{
    update();
}

void GfxWindow::clearColorChanged(QColor color)
{
    mClearColor = ToGfx(color);
}

GfxWidget::GfxWidget(QWidget* parent) : QWidget(parent)
{
    mWindow = new GfxWindow();
    // the container takes ownership of the window.
    mContainer = QWidget::createWindowContainer(mWindow, this);
    mWindow->onPaintScene = [&] (gfx::Painter& painter, double secs){
        if (mWindow && onPaintScene)
            onPaintScene(painter, secs);
    };
    mWindow->onInitScene = [&](unsigned width, unsigned height) {
        if (mWindow && onInitScene)
            onInitScene(width, height);
    };
    mWindow->onMouseMove = [&](QMouseEvent* mickey) {
        if (mWindow && onMouseMove)
            onMouseMove(mickey);
    };
    mWindow->onMousePress = [&](QMouseEvent* mickey) {
        if (mWindow && onMousePress)
            onMousePress(mickey);
    };
    mWindow->onMouseRelease = [&](QMouseEvent* mickey) {
        if (mWindow && onMouseRelease)
            onMouseRelease(mickey);
    };
    mWindow->onKeyPress = [&](QKeyEvent* key) {
        // The context menu can no longer be used since QWindow
        // doesn't support it. Also we cannot use the context
        // menu with the container widget either because this
        // window then renders on top obscuring the context menu (?)
        // TODO: Ultimately some better UI means are needed for these
        // options.
        if (key->key() == Qt::Key_F1)
            mWindow->toggleShowFps();
        else if (key->key() == Qt::Key_F2)
            showColorDialog();

        if (mWindow && onKeyPress)
            return onKeyPress(key);
        return false;
    };
    mWindow->onMouseWheel = [&](QWheelEvent* wheel) {
        translateZoomInOut(wheel);
    };
}

GfxWidget::~GfxWidget()
{
    DEBUG("Destroy GfxWidget");
}

void GfxWidget::dispose()
{
    // dispose of the graphics resources.
    mWindow->dispose();
    DEBUG("Disposed GfxWindow.");
}

void GfxWidget::resizeEvent(QResizeEvent* resize)
{
    // resize the container to be same size as this shim
    // widget.
    // the container has taken ownership of the window and
    // will then in turn resize the window.
    mContainer->resize(resize->size());
}

void GfxWidget::showColorDialog()
{
    const gfx::Color4f current = mWindow->getClearColor();

    color_widgets::ColorDialog dlg(this);
    dlg.setColor(FromGfx(current));
    connect(&dlg, &color_widgets::ColorDialog::colorChanged,
            mWindow, &GfxWindow::clearColorChanged);

    if (dlg.exec() == QDialog::Rejected)
    {
        mWindow->setClearColor(current);
        return;
    }
    mWindow->setClearColor(ToGfx(dlg.color()));
}

void GfxWidget::translateZoomInOut(QWheelEvent* wheel)
{
    const auto mods = wheel->modifiers();
    if (mods != Qt::ControlModifier)
        return;

    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    //DEBUG("Zoom steps: %1", num_zoom_steps);

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0 && onZoomIn)
            onZoomIn();
        else if (num_zoom_steps < 0 && onZoomOut)
            onZoomOut();
    }
}

} // namespace

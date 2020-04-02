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

#pragma once

#include "config.h"

#include "warnpush.h"
#  include "ui_animationwidget.h"
#include "warnpop.h"

#include "editor/gui/mainwidget.h"
#include "scene/animation.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class AnimationWidget : public MainWidget
    {
        Q_OBJECT
    public:
        AnimationWidget(app::Workspace* workspace);
        AnimationWidget(app::Workspace* workspace, const app::Resource& resource);
       ~AnimationWidget();

        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionNewRect_triggered();
        void on_actionDeleteComponent_triggered();
        void on_components_customContextMenuRequested(QPoint);
        void on_plus90_clicked();
        void on_minus90_clicked();
        void on_resetTransform_clicked();
        void on_materials_currentIndexChanged(const QString& name);
        void on_renderPass_currentIndexChanged(const QString& name);
        void on_layer_valueChanged(int layer);

        void currentComponentRowChanged(const QModelIndex& current, const QModelIndex& previous);

    private:
        void paintScene(gfx::Painter& painter, double secs);
        scene::Animation::Component* GetCurrentComponent();
    private:
        class ComponentModel;
        class Tool;
        class PlaceRectTool;
        class CameraTool;

    private:
        Ui::AnimationWidget mUI;
    private:
        // current tool (if any, can be nullptr when no tool is selected).
        std::unique_ptr<Tool> mCurrentTool;
    private:
        // state shared with the tools is packed inside a single
        // struct type for convenience
        struct State {
            scene::Animation animation;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            std::unique_ptr<ComponentModel> model;
            // current workspace we're editing.
            app::Workspace* workspace = nullptr;
        } mState;
        // current time of the animation. accumulates when running.
        float mTime = 0.0f;
        // possible states of the animation playback.
        enum PlayState {
            Playing, Paused, Stopped
        };
        // current animation playback state.
        PlayState mPlayState = PlayState::Stopped;
    };
} // namespace

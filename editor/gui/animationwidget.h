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
#  include <QMenu>
#include "warnpop.h"

#include "editor/gui/mainwidget.h"
#include "gamelib/animation.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
} // app

namespace gui
{
    class TreeWidget;

    class AnimationWidget : public MainWidget
    {
        Q_OBJECT
    public:
        AnimationWidget(app::Workspace* workspace);
        AnimationWidget(app::Workspace* workspace, const app::Resource& resource);
       ~AnimationWidget();

        virtual void addActions(QToolBar& bar) override;
        virtual void addActions(QMenu& menu) override;
        virtual bool saveState(Settings& settings) const override;
        virtual bool loadState(const Settings& settings) override;
        virtual void zoomIn() override;
        virtual void zoomOut() override;
        virtual void reloadShaders() override;
        virtual void reloadTextures() override;
        virtual void shutdown() override;
        virtual void animate(double secs) override;
        virtual void setTargetFps(unsigned fps) override;
        virtual bool confirmClose() override;

    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNewRect_triggered();
        void on_actionNewCircle_triggered();
        void on_actionNewTriangle_triggered();
        void on_actionNewArrow_triggered();
        void on_actionNewRoundRect_triggered();
        void on_actionDeleteComponent_triggered();
        void on_tree_customContextMenuRequested(QPoint);
        void on_plus90_clicked();
        void on_minus90_clicked();
        void on_resetTransform_clicked();
        void on_cPlus90_clicked();
        void on_cMinus90_clicked();
        void on_materials_currentIndexChanged(const QString& name);
        void on_drawables_currentIndexChanged(const QString& name);
        void on_renderPass_currentIndexChanged(const QString& name);
        void on_renderStyle_currentIndexChanged(const QString& name);
        void on_layer_valueChanged(int layer);
        void on_lineWidth_valueChanged(double value);
        void on_cSizeX_valueChanged(double value);
        void on_cSizeY_valueChanged(double value);
        void on_cTranslateX_valueChanged(double value);
        void on_cTranslateY_valueChanged(double value);
        void on_cRotation_valueChanged(double value);
        void on_cName_textChanged(const QString& text);
        void on_chkUpdateMaterial_stateChanged(int);
        void on_chkUpdateDrawable_stateChanged(int);
        void on_chkDoesRender_stateChanged(int);

        void currentComponentRowChanged();
        void placeNewParticleSystem();
        void newResourceAvailable(const app::Resource* resource);
        void resourceToBeDeleted(const app::Resource* resource);
        void treeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void treeClickEvent(TreeWidget::TreeItem* item);

    private:
        void paintScene(gfx::Painter& painter, double secs);
        game::AnimationNode* GetCurrentNode();
        void updateCurrentNodeProperties();
        void updateCurrentNodePosition(float dx, float dy);
    private:
        class TreeModel;
        class Tool;
        class PlaceTool;
        class CameraTool;
        class MoveTool;
        class ResizeTool;
        class RotateTool;

    private:
        Ui::AnimationWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for user defined drawables
        QMenu* mDrawableMenu = nullptr;
    private:
        // current tool (if any, can be nullptr when no tool is selected).
        std::unique_ptr<Tool> mCurrentTool;
    private:
        // state shared with the tools is packed inside a single
        // struct type for convenience
        struct State {
            game::Animation animation;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;

            // current workspace we're editing.
            app::Workspace* workspace = nullptr;
            //
            gui::TreeWidget* scenegraph_tree_view = nullptr;

            TreeModel* scenegraph_tree_model = nullptr;
        } mState;

        // tree model for accessing the animations' render tree
        // data from the tree widget.
        std::unique_ptr<TreeModel> mTreeModel;
        // the original hash value that is used to
        // check against if there are unsaved changes.
        std::size_t mOriginalHash = 0;
        // current time of the animation. accumulates when running.
        float mTime = 0.0f;
        // possible states of the animation playback.
        enum PlayState {
            Playing, Paused, Stopped
        };
        // current animation playback state.
        PlayState mPlayState = PlayState::Stopped;

        bool mCameraWasLoaded = false;
    };
} // namespace

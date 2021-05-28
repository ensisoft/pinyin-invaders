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
#  include "ui_scenewidget.h"
#  include <QMenu>
#  include <boost/circular_buffer.hpp>
#include "warnpop.h"

#include <memory>
#include <string>

#include "editor/gui/mainwidget.h"
#include "editor/gui/treemodel.h"
#include "engine/scene.h"
#include "engine/renderer.h"

namespace gfx {
    class Painter;
} // gfx

namespace app {
    class Resource;
    class Workspace;
};

namespace gui
{
    class TreeWidget;
    class MouseTool;

    class SceneWidget : public MainWidget
    {
        Q_OBJECT
    public:
        SceneWidget(app::Workspace* workspace);
        SceneWidget(app::Workspace* workspace, const app::Resource& resource);
       ~SceneWidget();

        virtual void AddActions(QToolBar& bar) override;
        virtual void AddActions(QMenu& menu) override;
        virtual bool SaveState(Settings& settings) const override;
        virtual bool LoadState(const Settings& settings) override;
        virtual bool CanTakeAction(Actions action, const Clipboard* clipboard) const override;
        virtual void Cut(Clipboard& clipboard) override;
        virtual void Copy(Clipboard& clipboard) const override;
        virtual void Paste(const Clipboard& clipboard) override;
        virtual void Undo() override;
        virtual void Save() override;
        virtual void ZoomIn() override;
        virtual void ZoomOut() override;
        virtual void ReloadShaders() override;
        virtual void ReloadTextures() override;
        virtual void Shutdown() override;
        virtual void Update(double secs) override;
        virtual void Render() override;
        virtual bool HasUnsavedChanges() const override;
        virtual bool ConfirmClose() override;
        virtual void Refresh() override;
        virtual bool GetStats(Stats* stats) const override;
    private slots:
        void on_actionPlay_triggered();
        void on_actionPause_triggered();
        void on_actionStop_triggered();
        void on_actionSave_triggered();
        void on_actionNodeDelete_triggered();
        void on_actionNodeBreakLink_triggered();
        void on_actionNodePlace_triggered();
        void on_actionNodeDuplicate_triggered();
        void on_actionNodeMoveUpLayer_triggered();
        void on_actionNodeMoveDownLayer_triggered();
        void on_btnNewScriptVar_clicked();
        void on_btnEditScriptVar_clicked();
        void on_btnDeleteScriptVar_clicked();
        void on_btnViewPlus90_clicked();
        void on_btnViewMinus90_clicked();
        void on_btnViewReset_clicked();
        void on_nodeName_textChanged(const QString& text);
        void on_nodeEntity_currentIndexChanged(const QString& name);
        void on_nodeLayer_valueChanged(int layer);
        void on_nodeLink_currentIndexChanged(const QString& text);
        void on_nodeIsVisible_stateChanged(int);
        void on_nodeTranslateX_valueChanged(double value);
        void on_nodeTranslateY_valueChanged(double value);
        void on_nodeScaleX_valueChanged(double value);
        void on_nodeScaleY_valueChanged(double value);
        void on_nodeRotation_valueChanged(double value);
        void on_btnEntityParams_clicked();
        void on_btnNodePlus90_clicked();
        void on_btnNodeMinus90_clicked();
        void on_tree_customContextMenuRequested(QPoint);

        void PlaceNewEntity();
        void TreeCurrentNodeChangedEvent();
        void TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target);
        void TreeClickEvent(TreeWidget::TreeItem* item);
        void NewResourceAvailable(const app::Resource* resource);
        void ResourceToBeDeleted(const app::Resource* resource);
        void ResourceUpdated(const app::Resource* resource);
    private:
        void InitScene(unsigned width, unsigned height);
        void PaintScene(gfx::Painter& painter, double secs);
        void MouseMove(QMouseEvent* mickey);
        void MousePress(QMouseEvent* mickey);
        void MouseRelease(QMouseEvent* mickey);
        void MouseWheel(QWheelEvent* wheel);
        bool KeyPress(QKeyEvent* key);
        void DisplayCurrentCameraLocation();
        void DisplayCurrentNodeProperties();
        void UncheckPlacementActions();
        void TranslateCurrentNode(float dx, float dy);
        void TranslateCamera(float dx, float dy);
        void RebuildMenus();
        void RebuildCombos();
        void UpdateResourceReferences();
        game::SceneNodeClass* SelectNode(const QPoint& click_point, glm::vec2* hitpos);
        game::SceneNodeClass* GetCurrentNode();
        const game::SceneNodeClass* GetCurrentNode() const;
    private:
        Ui::SceneWidget mUI;
        // there doesn't seem to be a way to do this in the designer
        // so we create our menu for Entities.
        QMenu* mEntities = nullptr;
    private:
        class PlaceEntityTool;
        class ScriptVarModel;
        enum class PlayState {
            Playing, Paused, Stopped
        };
        struct State {
            game::SceneClass scene;
            game::Renderer renderer;
            app::Workspace* workspace = nullptr;
            float camera_offset_x = 0.0f;
            float camera_offset_y = 0.0f;
            TreeWidget* view = nullptr;
            QString last_placed_entity;
        } mState;

        // Tree model impl for displaying scene's render tree
        // in the tree widget.
        using TreeModel = RenderTreeModel<game::SceneClass>;
        std::size_t mOriginalHash = 0;
        std::unique_ptr<TreeModel> mRenderTree;
        std::unique_ptr<MouseTool> mCurrentTool;
        std::unique_ptr<ScriptVarModel> mScriptVarModel;
        PlayState mPlayState = PlayState::Stopped;
        double mCurrentTime = 0.0;
        double mSceneTime   = 0.0;
        double mViewTransformStartTime = 0.0;
        float mViewTransformRotation = 0.0f;
        bool mCameraWasLoaded = false;

        // Undo "stack" with fixed capacity that begins
        // overwrite old items when space is exceeded
        boost::circular_buffer<game::SceneClass> mUndoStack;
    };
}

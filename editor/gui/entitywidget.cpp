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

#define LOGTAG "entity"

#include "config.h"

#include "warnpush.h"
#  include <QPoint>
#  include <QMouseEvent>
#  include <QMessageBox>
#  include <base64/base64.h>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <algorithm>
#include <unordered_set>
#include <cmath>

#include "editor/app/eventlog.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/treewidget.h"
#include "editor/gui/treemodel.h"
#include "editor/gui/tool.h"
#include "editor/gui/entitywidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"

namespace gui
{

class EntityWidget::PlaceShapeTool : public MouseTool
{
public:
    PlaceShapeTool(EntityWidget::State& state, const QString& material, const QString& drawable)
            : mState(state)
            , mMaterialName(material)
            , mDrawableName(drawable)
    {
        mDrawableClass = mState.workspace->GetDrawableClassByName(mDrawableName);
        mMaterialClass = mState.workspace->GetMaterialClassByName(mMaterialName);
        mMaterial = gfx::CreateMaterialInstance(mMaterialClass);
        mDrawable = gfx::CreateDrawableInstance(mDrawableClass);
    }
    virtual void Render(gfx::Painter& painter, gfx::Transform& view) const
    {
        if (!mEngaged)
            return;

        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return;

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width  = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        view.Push();
        view.Scale(width, height);
        view.Translate(xpos, ypos);
        painter.Draw(*mDrawable, view, *mMaterial);

        // draw a selection rect around it.
        painter.Draw(gfx::Rectangle(gfx::Drawable::Style::Outline),
                     view, gfx::SolidColor(gfx::Color::Green));
        view.Pop();
    }
    virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view)
    {
        if (!mEngaged)
            return;

        const auto& view_to_model = glm::inverse(view.GetAsMatrix());
        const auto& p = mickey->pos();
        mCurrent = view_to_model * glm::vec4(p.x(), p.y(), 1.0f, 1.0f);
        mAlwaysSquare = mickey->modifiers() & Qt::ControlModifier;
    }
    virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) override
    {
        const auto button = mickey->button();
        if (button == Qt::LeftButton)
        {
            const auto& view_to_model = glm::inverse(view.GetAsMatrix());
            const auto& p = mickey->pos();
            mStart = view_to_model * glm::vec4(p.x(), p.y(), 1.0f, 1.0f);
            mCurrent = mStart;
            mEngaged = true;
        }
    }
    virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view)
    {
        const auto button = mickey->button();
        if (button != Qt::LeftButton)
            return false;

        ASSERT(mEngaged);

        mEngaged = false;
        const auto& diff = mCurrent - mStart;
        if (diff.x <= 0.0f || diff.y <= 0.0f)
            return false;

        std::string name;
        for (size_t i=0; i<666666; ++i)
        {
            name = "Node " + std::to_string(i);
            if (CheckNameAvailability(name))
                break;
        }

        const float xpos = mStart.x;
        const float ypos = mStart.y;
        const float hypotenuse = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        const float width = mAlwaysSquare ? hypotenuse : diff.x;
        const float height = mAlwaysSquare ? hypotenuse : diff.y;

        game::DrawableItemClass item;
        item.SetMaterialId(mMaterialClass->GetId());
        item.SetDrawableId(mDrawableClass->GetId());
        game::EntityNodeClass node;
        node.SetDrawable(item);
        node.SetName(name);
        node.SetTranslation(glm::vec2(xpos + 0.5*width, ypos + 0.5*height));
        node.SetSize(glm::vec2(width, height));
        node.SetScale(glm::vec2(1.0f, 1.0f));

        // by default we're appending to the root item.
        auto& root  = mState.entity.GetRenderTree();
        auto* child = mState.entity.AddNode(std::move(node));
        root.AppendChild(child);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(child->GetClassId()));
        DEBUG("Added new shape '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.entity.GetNumNodes(); ++i)
        {
            const auto& node = mState.entity.GetNode(i);
            if (node.GetName() == name)
                return false;
        }
        return true;
    }
private:
    EntityWidget::State& mState;
    // the starting object position in model coordinates of the placement
    // based on the mouse position at the time.
    glm::vec4 mStart;
    // the current object ending position in model coordinates.
    // the object occupies the rectangular space between the start
    // and current positions on the X and Y axis.
    glm::vec4 mCurrent;
    bool mEngaged = false;
    bool mAlwaysSquare = false;
private:
    QString mMaterialName;
    QString mDrawableName;
    std::shared_ptr<const gfx::DrawableClass> mDrawableClass;
    std::shared_ptr<const gfx::MaterialClass> mMaterialClass;
    std::unique_ptr<gfx::Material> mMaterial;
    std::unique_ptr<gfx::Drawable> mDrawable;
};


EntityWidget::EntityWidget(app::Workspace* workspace)
{
    DEBUG("Create EntityWidget");

    mRenderTree.reset(new TreeModel(mState.entity));

    mUI.setupUi(this);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.widget->onZoomIn  = std::bind(&EntityWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&EntityWidget::ZoomOut, this);
    mUI.widget->onPaintScene = std::bind(&EntityWidget::PaintScene, this,
                                         std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onMouseMove = std::bind(&EntityWidget::MouseMove, this,
                                        std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&EntityWidget::MousePress, this,
                                         std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&EntityWidget::MouseRelease, this,
                                           std::placeholders::_1);
    mUI.widget->onKeyPress = std::bind(&EntityWidget::KeyPress, this,
                                       std::placeholders::_1);
    mUI.widget->onInitScene = std::bind(&EntityWidget::InitScene, this,
                                        std::placeholders::_1, std::placeholders::_2);

    // create the menu for creating instances of user defined drawables
    // since there doesn't seem to be a way to do this in the designer.
    mParticleSystems = new QMenu(this);
    mParticleSystems->menuAction()->setIcon(QIcon("icons:particle.png"));
    mParticleSystems->menuAction()->setText("Particle");
    mCustomShapes = new QMenu(this);
    mCustomShapes->menuAction()->setIcon(QIcon("icons:polygon.png"));
    mCustomShapes->menuAction()->setText("Polygon");

    mState.workspace = workspace;
    mState.renderer.SetLoader(workspace);
    mState.view = mUI.tree;

    // connect tree widget signals
    connect(mUI.tree, &TreeWidget::currentRowChanged, this, &EntityWidget::TreeCurrentNodeChangedEvent);
    connect(mUI.tree, &TreeWidget::dragEvent,  this, &EntityWidget::TreeDragEvent);
    connect(mUI.tree, &TreeWidget::clickEvent, this, &EntityWidget::TreeClickEvent);
    // connect workspace signals for resource management
    connect(workspace, &app::Workspace::NewResourceAvailable, this, &EntityWidget::NewResourceAvailable);
    connect(workspace, &app::Workspace::ResourceToBeDeleted,  this, &EntityWidget::ResourceToBeDeleted);
    connect(workspace, &app::Workspace::ResourceUpdated,      this, &EntityWidget::ResourceUpdated);

    PopulateFromEnum<GridDensity>(mUI.cmbGrid);
    PopulateFromEnum<game::DrawableItemClass::RenderPass>(mUI.dsRenderPass);
    PopulateFromEnum<game::DrawableItemClass::RenderStyle>(mUI.dsRenderStyle);
    PopulateFromEnum<game::RigidBodyItemClass::Simulation>(mUI.rbSimulation);
    PopulateFromEnum<game::RigidBodyItemClass::CollisionShape>(mUI.rbShape);
    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.name, QString("My Entity"));
    SetValue(mUI.ID, mState.entity.GetId());
    setWindowTitle("My Entity");

    RebuildMenus();
    RebuildCombos();
}

EntityWidget::EntityWidget(app::Workspace* workspace, const app::Resource& resource)
  : EntityWidget(workspace)
{
    DEBUG("Editing entity '%1'", resource.GetName());
    const game::EntityClass* content = nullptr;
    resource.GetContent(&content);

    mState.entity = *content;
    mOriginalHash = mState.entity.GetHash();
    mCameraWasLoaded = true;

    SetValue(mUI.name, resource.GetName());
    SetValue(mUI.ID, content->GetId());
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x);
    GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);
    setWindowTitle(resource.GetName());

    UpdateDeletedResourceReferences();

    mRenderTree.reset(new TreeModel(mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

EntityWidget::~EntityWidget()
{
    DEBUG("Destroy EntityWidget");
}

void EntityWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
    bar.addSeparator();
    bar.addAction(mUI.actionNewRect);
    bar.addAction(mUI.actionNewRoundRect);
    bar.addAction(mUI.actionNewCircle);
    bar.addAction(mUI.actionNewIsoscelesTriangle);
    bar.addAction(mUI.actionNewRightTriangle);
    bar.addAction(mUI.actionNewTrapezoid);
    bar.addAction(mUI.actionNewParallelogram);
    bar.addAction(mUI.actionNewCapsule);
    bar.addSeparator();
    bar.addAction(mCustomShapes->menuAction());
    bar.addSeparator();
    bar.addAction(mParticleSystems->menuAction());
}
void EntityWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
    menu.addSeparator();
    menu.addAction(mUI.actionNewRect);
    menu.addAction(mUI.actionNewRoundRect);
    menu.addAction(mUI.actionNewCircle);
    menu.addAction(mUI.actionNewIsoscelesTriangle);
    menu.addAction(mUI.actionNewRightTriangle);
    menu.addAction(mUI.actionNewTrapezoid);
    menu.addAction(mUI.actionNewParallelogram);
    menu.addAction(mUI.actionNewCapsule);
    menu.addSeparator();
    menu.addAction(mCustomShapes->menuAction());
    menu.addSeparator();
    menu.addAction(mParticleSystems->menuAction());
}

bool EntityWidget::SaveState(Settings& settings) const
{
    settings.saveWidget("Entity", mUI.name);
    settings.saveWidget("Entity", mUI.ID);
    settings.saveWidget("Entity", mUI.scaleX);
    settings.saveWidget("Entity", mUI.scaleY);
    settings.saveWidget("Entity", mUI.rotation);
    settings.saveWidget("Entity", mUI.chkShowOrigin);
    settings.saveWidget("Entity", mUI.chkShowGrid);
    settings.saveWidget("Entity", mUI.cmbGrid);
    settings.saveWidget("Entity", mUI.zoom);
    settings.saveWidget("Entity", mUI.widget);
    settings.setValue("Entity", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("Entity", "camera_offset_y", mState.camera_offset_y);
    // the entity can already serialize into JSON.
    // so let's use the JSON serialization in the entity
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mState.entity.ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Entity", "content", base64);
    return true;
}
bool EntityWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("Entity", mUI.name);
    settings.loadWidget("Entity", mUI.ID);
    settings.loadWidget("Entity", mUI.scaleX);
    settings.loadWidget("Entity", mUI.scaleY);
    settings.loadWidget("Entity", mUI.rotation);
    settings.loadWidget("Entity", mUI.chkShowOrigin);
    settings.loadWidget("Entity", mUI.chkShowGrid);
    settings.loadWidget("Entity", mUI.cmbGrid);
    settings.loadWidget("Entity", mUI.zoom);
    settings.loadWidget("Entity", mUI.widget);
    setWindowTitle(mUI.name->text());
    mState.camera_offset_x = settings.getValue("Entity", "camera_offset_x", mState.camera_offset_x);
    mState.camera_offset_y = settings.getValue("Entity", "camera_offset_y", mState.camera_offset_y);
    // set a flag to *not* adjust the camera on gfx widget init to the middle the of widget.
    mCameraWasLoaded = true;

    const std::string& base64 = settings.getValue("Entity", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret  = game::EntityClass::FromJson(json);
    if (!ret.has_value())
    {
        ERROR("Failed to load entity widget state.");
        return false;
    }
    mState.entity  = std::move(ret.value());
    mOriginalHash = mState.entity.GetHash();

    UpdateDeletedResourceReferences();

    mRenderTree.reset(new TreeModel(mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}
bool EntityWidget::CanZoomIn() const
{
    const auto max = mUI.zoom->maximum();
    const auto val = mUI.zoom->value();
    return val < max;
}
bool EntityWidget::CanZoomOut() const
{
    const auto min = mUI.zoom->minimum();
    const auto val = mUI.zoom->value();
    return val > min;
}
void EntityWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}
void EntityWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value - 0.1);
}
void EntityWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void EntityWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}
void EntityWidget::Shutdown()
{
    mUI.widget->dispose();
}
void EntityWidget::Update(double secs)
{
    if (mPlayState == PlayState::Playing)
    {
        // todo: update scene?
        mState.renderer.Update(mState.entity, mEntityTime, secs);
        mEntityTime += secs;
        mUI.time->setText(QString::number(mEntityTime));
    }
    mCurrentTime += secs;
}
void EntityWidget::Render()
{
    mUI.widget->triggerPaint();
}
bool EntityWidget::ConfirmClose()
{
    const auto hash = mState.entity.GetHash();
    if (hash == mOriginalHash)
        return true;

    QMessageBox msg(this);
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    msg.setIcon(QMessageBox::Question);
    msg.setText(tr("Looks like you have unsaved changes. Would you like to save them?"));
    const auto ret = msg.exec();
    if (ret == QMessageBox::Cancel)
        return false;
    else if (ret == QMessageBox::No)
        return true;

    on_actionSave_triggered();
    return true;
}
void EntityWidget::Refresh()
{

}

void EntityWidget::on_actionPlay_triggered()
{
    mPlayState = PlayState::Playing;
    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}
void EntityWidget::on_actionPause_triggered()
{
    mPlayState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}
void EntityWidget::on_actionStop_triggered()
{
    mEntityTime = 0.0f;
    mPlayState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.time->clear();
}
void EntityWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.name))
        return;
    const QString& name = GetValue(mUI.name);
    app::EntityResource resource(mState.entity, name);
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "widget", mUI.widget);

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.entity.GetHash();

    INFO("Saved entity '%1'", name);
    NOTE("Saved entity '%1'", name);
    setWindowTitle(name);
}
void EntityWidget::on_actionNewRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "Rectangle"));

    UncheckPlacementActions();
    mUI.actionNewRect->setChecked(true);
}
void EntityWidget::on_actionNewCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "Circle"));

    UncheckPlacementActions();
    mUI.actionNewCircle->setChecked(true);
}
void EntityWidget::on_actionNewIsoscelesTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "IsoscelesTriangle"));

    UncheckPlacementActions();
    mUI.actionNewIsoscelesTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRightTriangle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "RightTriangle"));

    UncheckPlacementActions();
    mUI.actionNewRightTriangle->setChecked(true);
}
void EntityWidget::on_actionNewRoundRect_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "RoundRect"));

    UncheckPlacementActions();
    mUI.actionNewRoundRect->setChecked(true);
}
void EntityWidget::on_actionNewTrapezoid_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "Trapezoid"));

    UncheckPlacementActions();
    mUI.actionNewTrapezoid->setChecked(true);
}
void EntityWidget::on_actionNewCapsule_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "Capsule"));

    UncheckPlacementActions();
    mUI.actionNewCapsule->setChecked(true);
}
void EntityWidget::on_actionNewParallelogram_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "Parallelogram"));

    UncheckPlacementActions();
    mUI.actionNewParallelogram->setChecked(true);
}

void EntityWidget::on_actionNodeDelete_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        mState.entity.DeleteNode(node);

        mUI.tree->Rebuild();
    }
}
void EntityWidget::on_actionNodeMoveUpLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer + 1);
        }
    }
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_actionNodeMoveDownLayer_triggered()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const int layer = item->GetLayer();
            item->SetLayer(layer - 1);
        }
    }
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_actionNodeDuplicate_triggered()
{
    if (const auto* node = GetCurrentNode())
    {
        auto* dupe = mState.entity.DuplicateNode(node);
        // update the the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(dupe->GetClassId()));
    }
}

void EntityWidget::on_plus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void EntityWidget::on_minus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void EntityWidget::on_resetTransform_clicked()
{
    const auto width = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto rotation = mUI.rotation->value();
    mState.camera_offset_x = width * 0.5f;
    mState.camera_offset_y = height * 0.5f;
    mViewTransformRotation = rotation;
    mViewTransformStartTime = mCurrentTime;
    // set new camera offset to the center of the widget.
    mUI.translateX->setValue(0);
    mUI.translateY->setValue(0);
    mUI.scaleX->setValue(1.0f);
    mUI.scaleY->setValue(1.0f);
    mUI.rotation->setValue(0);
}

void EntityWidget::on_nodeName_textChanged(const QString& text)
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return;
    else if (!item->GetUserData())
        return;
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    node->SetName(app::ToUtf8(text));
    item->SetText(text);
    mUI.tree->Update();
}

void EntityWidget::on_nodeIsVisible_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetFlag(game::EntityNodeClass::Flags::VisibleInGame, GetValue(mUI.nodeIsVisible));
    }
}

void EntityWidget::on_nodeSizeX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.x = value;
        node->SetSize(size);
    }
}
void EntityWidget::on_nodeSizeY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto size = node->GetSize();
        size.y = value;
        node->SetSize(size);
    }
}
void EntityWidget::on_nodeTranslateX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.x = value;
        node->SetTranslation(translate);
    }
}
void EntityWidget::on_nodeTranslateY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto translate = node->GetTranslation();
        translate.y = value;
        node->SetTranslation(translate);
    }
}
void EntityWidget::on_nodeScaleX_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.x = value;
        node->SetScale(scale);
    }
}
void EntityWidget::on_nodeScaleY_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        auto scale = node->GetScale();
        scale.y = value;
        node->SetScale(scale);
    }
}
void EntityWidget::on_nodeRotation_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        node->SetRotation(qDegreesToRadians(value));
    }
}

void EntityWidget::on_nodePlus90_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const float value = GetValue(mUI.nodeRotation);
        // careful, triggers value changed event.
        mUI.nodeRotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    }
}
void EntityWidget::on_nodeMinus90_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        const float value = GetValue(mUI.nodeRotation);
        // careful, triggers value changed event.
        mUI.nodeRotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    }
}

void EntityWidget::on_dsDrawable_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetDrawableId(mState.workspace->GetDrawableClassByName(name)->GetId());
        }
    }
}
void EntityWidget::on_dsMaterial_currentIndexChanged(const QString& name)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetMaterialId(mState.workspace->GetMaterialClassByName(name)->GetId());
        }
    }
}
void EntityWidget::on_dsRenderPass_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetRenderPass(GetValue(mUI.dsRenderPass));
        }
    }
}
void EntityWidget::on_dsRenderStyle_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetRenderStyle(GetValue(mUI.dsRenderStyle));
        }
    }
}

void EntityWidget::on_dsLayer_valueChanged(int value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetLayer(value);
        }
    }
}

void EntityWidget::on_dsLineWidth_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetLineWidth(value);
        }
    }
}

void EntityWidget::on_dsUpdateDrawable_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, GetValue(mUI.dsUpdateDrawable));
        }
    }
}
void EntityWidget::on_dsUpdateMaterial_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetFlag(game::DrawableItemClass::Flags::UpdateMaterial, GetValue(mUI.dsUpdateMaterial));
        }
    }
}
void EntityWidget::on_dsRestartDrawable_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            item->SetFlag(game::DrawableItemClass::Flags::RestartDrawable, GetValue(mUI.dsRestartDrawable));
        }
    }
}
void EntityWidget::on_dsOverrideAlpha_stateChanged(int)
{
    UpdateCurrentNodeAlpha();
}

void EntityWidget::on_dsAlpha_valueChanged()
{
    UpdateCurrentNodeAlpha();
}

void EntityWidget::on_rbSimulation_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetSimulation(GetValue(mUI.rbSimulation));
        }
    }
}

void EntityWidget::on_rbShape_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetCollisionShape(GetValue(mUI.rbShape));
            DisplayCurrentNodeProperties();
        }
    }
}

void EntityWidget::on_rbPolygon_currentIndexChanged(const QString&)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            const QString& poly = GetValue(mUI.rbPolygon);
            body->SetPolygonShapeId(mState.workspace->GetDrawableClassByName(poly)->GetId());
        }
    }
}

void EntityWidget::on_rbFriction_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetFriction(GetValue(mUI.rbFriction));
        }
    }
}
void EntityWidget::on_rbRestitution_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetRestitution(GetValue(mUI.rbRestitution));
        }
    }
}
void EntityWidget::on_rbAngularDamping_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetAngularDamping(GetValue(mUI.rbAngularDamping));
        }
    }
}
void EntityWidget::on_rbLinearDamping_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetLinearDamping(GetValue(mUI.rbLinearDamping));
        }
    }
}
void EntityWidget::on_rbDensity_valueChanged(double value)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetDensity(GetValue(mUI.rbDensity));
        }
    }
}

void EntityWidget::on_rbIsBullet_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetFlag(game::RigidBodyItemClass::Flags::Bullet, GetValue(mUI.rbIsBullet));
        }
    }
}
void EntityWidget::on_rbIsSensor_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetFlag(game::RigidBodyItemClass::Flags::Sensor, GetValue(mUI.rbIsSensor));
        }
    }
}
void EntityWidget::on_rbIsEnabled_stateChanged(int)
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* body = node->GetRigidBody())
        {
            body->SetFlag(game::RigidBodyItemClass::Flags::Enabled, GetValue(mUI.rbIsEnabled));
        }
    }
}

void EntityWidget::on_drawableItem_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasDrawable())
            {
                game::DrawableItemClass draw;
                draw.SetMaterialId(mState.workspace->GetMaterialClassByName("Checkerboard")->GetId());
                draw.SetDrawableId(mState.workspace->GetDrawableClassByName("Rectangle")->GetId());
                node->SetDrawable(draw);
                DEBUG("Added drawable item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveDrawable();
            DEBUG("Removed drawable item from '%1'", node->GetName());
        }
    }
    DisplayCurrentNodeProperties();
}
void EntityWidget::on_rigidBodyItem_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasRigidBody())
            {
                game::RigidBodyItemClass body;
                node->SetRigidBody(body);
                DEBUG("Added rigid body to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveRigidBody();
            DEBUG("Removed rigid body from '%1'", node->GetName());
        }
    }
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_tree_customContextMenuRequested(QPoint)
{
    const auto* node = GetCurrentNode();
    const auto* item = node ? node->GetDrawable() : nullptr;

    mUI.actionNodeMoveDownLayer->setEnabled(item != nullptr);
    mUI.actionNodeMoveUpLayer->setEnabled(item != nullptr);
    mUI.actionNodeDelete->setEnabled(node != nullptr);
    mUI.actionNodeDuplicate->setEnabled(node != nullptr);

    QMenu menu(this);
    menu.addAction(mUI.actionNodeMoveUpLayer);
    menu.addAction(mUI.actionNodeMoveDownLayer);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDuplicate);
    menu.addSeparator();
    menu.addAction(mUI.actionNodeDelete);
    menu.exec(QCursor::pos());
}

void EntityWidget::TreeCurrentNodeChangedEvent()
{
    if (const auto* node = GetCurrentNode())
    {
        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform, true);
        SetEnabled(mUI.nodeItems, true);
    }
    else
    {
        SetEnabled(mUI.nodeProperties, false);
        SetEnabled(mUI.nodeTransform, false);
        SetEnabled(mUI.nodeItems, false);
    }
    DisplayCurrentNodeProperties();
}
void EntityWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.entity.GetRenderTree();
    auto* src_value = static_cast<game::EntityNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::EntityNodeClass*>(target->GetUserData());

    // find the game graph node that contains this AnimationNode.
    auto* src_node   = tree.FindNodeByValue(src_value);
    auto* src_parent = tree.FindParent(src_node);

    // check if we're trying to drag a parent onto its own child
    if (src_node->FindNodeByValue(dst_value))
        return;

    game::EntityClass::RenderTreeNode branch = *src_node;
    src_parent->DeleteChild(src_node);

    auto* dst_node  = tree.FindNodeByValue(dst_value);
    dst_node->AppendChild(std::move(branch));
}
void EntityWidget::TreeClickEvent(TreeWidget::TreeItem* item)
{
    //DEBUG("Tree click event: %1", item->GetId());
    auto* node = static_cast<game::EntityNodeClass*>(item->GetUserData());
    if (node == nullptr)
        return;

    const bool visibility = node->TestFlag(game::EntityNodeClass::Flags::VisibleInEditor);
    node->SetFlag(game::EntityNodeClass::Flags::VisibleInEditor, !visibility);
    item->SetIconMode(visibility ? QIcon::Disabled : QIcon::Normal);
}

void EntityWidget::NewResourceAvailable(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
}
void EntityWidget::ResourceToBeDeleted(const app::Resource* resource)
{
    UpdateDeletedResourceReferences();
    RebuildCombos();
    RebuildMenus();
    DisplayCurrentNodeProperties();
}
void EntityWidget::ResourceUpdated(const app::Resource* resource)
{
    RebuildCombos();
    RebuildMenus();
}

void EntityWidget::PlaceNewParticleSystem()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the text in the action as the name of the drawable.
    const auto drawable = action->text();

    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceByName(drawable, app::Resource::Type::ParticleSystem);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceShapeTool(mState, material, drawable));
}
void EntityWidget::PlaceNewCustomShape()
{
    const auto* action = qobject_cast<QAction*>(sender());
    // using the text in the action as the name of the drawable.
    const auto drawable = action->text();
    // check the resource in order to get the default material name set in the
    // particle editor.
    const auto& resource = mState.workspace->GetResourceByName(drawable, app::Resource::Type::CustomShape);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceShapeTool(mState, material, drawable));
}

void EntityWidget::InitScene(unsigned width, unsigned height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = width * 0.5;
        mState.camera_offset_y = height * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void EntityWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    gfx::Transform view;
    view.Push();
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        const float zoom = GetValue(mUI.zoom);
        const float xs = GetValue(mUI.scaleX);
        const float ys = GetValue(mUI.scaleY);
        const GridDensity grid = GetValue(mUI.cmbGrid);
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    DrawHook hook(GetCurrentNode(), mPlayState == PlayState::Playing);

    // begin the animation transformation space
    view.Push();
        mState.renderer.BeginFrame();
        mState.renderer.Draw(mState.entity, painter, view, &hook);
        mState.renderer.EndFrame();
    view.Pop();

    if (mCurrentTool)
        mCurrentTool->Render(painter, view);

    // right arrow
    if (GetValue(mUI.chkShowOrigin))
    {
        DrawBasisVectors(painter, view);
    }

    // pop view transformation
    view.Pop();
}

void EntityWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x, mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey, view);
    }
    // update the properties that might have changed as the result of application
    // of the current tool.
    DisplayCurrentCameraLocation();
    DisplayCurrentNodeProperties();
}
void EntityWidget::MousePress(QMouseEvent* mickey)
{
    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate((float)qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (!mCurrentTool)
    {
        // On a mouse press start we want to select the tool based on where the pointer
        // is and which object it interacts with in the game when mouse press starts.
        //
        // If the mouse pointer doesn't intersect with an object we create a new
        // camera tool for moving the viewport and object selection gets cleared.
        //
        // If the mouse pointer intersects with an object that is the same object
        // that was already selected:
        // Check if the pointer intersects with one of the resizing boxes inside
        // the object's selection box. If it does then we create a new resizing tool
        // otherwise we create a new move tool instance for moving the object.
        //
        // If the mouse pointer intersects with an object that is not the same object
        // that was previously selected:
        // Select the object.
        //

        // take the widget space mouse coordinate and transform into view/camera space.
        const auto mouse_widget_position_x = mickey->pos().x();
        const auto mouse_widget_position_y = mickey->pos().y();
        const auto &widget_to_view = glm::inverse(view.GetAsMatrix());
        const auto &mouse_view_position = widget_to_view * glm::vec4(mouse_widget_position_x,
                                                                     mouse_widget_position_y, 1.0f, 1.0f);

        std::vector<game::EntityNodeClass *> nodes_hit;
        std::vector<glm::vec2> hitbox_coords;
        mState.entity.CoarseHitTest(mouse_view_position.x, mouse_view_position.y, &nodes_hit, &hitbox_coords);

        // if nothing was hit clear the selection.
        if (nodes_hit.empty())
        {
            mUI.tree->ClearSelection();
            mCurrentTool.reset(new MoveCameraTool(mState));
        }
        else
        {
            const TreeWidget::TreeItem *selected = mUI.tree->GetSelectedItem();
            const game::EntityNodeClass *previous = selected
                ? static_cast<const game::EntityNodeClass *>(selected->GetUserData())
                : nullptr;

            // if the currently selected node is among the ones being hit
            // then retain that selection.
            // otherwise select the last one of the list. (the rightmost child)
            game::EntityNodeClass *hit = nodes_hit.back();
            glm::vec2 hitpos = hitbox_coords.back();
            for (size_t i = 0; i < nodes_hit.size(); ++i)
            {
                if (nodes_hit[i] == previous)
                {
                    hit = nodes_hit[i];
                    hitpos = hitbox_coords[i];
                    break;
                }
            }

            const auto &size = hit->GetSize();
            // check if any particular special area of interest is being hit
            const bool bottom_right_hitbox_hit = hitpos.x >= size.x - 10.0f &&
                                                 hitpos.y >= size.y - 10.0f;
            const bool top_left_hitbox_hit = hitpos.x >= 0 && hitpos.x <= 10.0f &&
                                             hitpos.y >= 0 && hitpos.y <= 10.0f;
            if (bottom_right_hitbox_hit)
                mCurrentTool.reset(new ResizeRenderTreeNodeTool(mState.entity, hit));
            else if (top_left_hitbox_hit)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(mState.entity, hit));
            else mCurrentTool.reset(new MoveRenderTreeNodeTool(mState.entity, hit));

            mUI.tree->SelectItemById(app::FromUtf8(hit->GetClassId()));
        }
    }
    if (mCurrentTool)
        mCurrentTool->MousePress(mickey, view);
}
void EntityWidget::MouseRelease(QMouseEvent* mickey)
{
    if (!mCurrentTool)
        return;

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    mCurrentTool->MouseRelease(mickey, view);
    mCurrentTool.release();

    UncheckPlacementActions();
}

bool EntityWidget::KeyPress(QKeyEvent* key)
{
    switch (key->key()) {
        case Qt::Key_Delete:
            on_actionNodeDelete_triggered();
            break;
        case Qt::Key_W:
            mState.camera_offset_y += 20.0f;
            break;
        case Qt::Key_S:
            mState.camera_offset_y -= 20.0f;
            break;
        case Qt::Key_A:
            mState.camera_offset_x += 20.0f;
            break;
        case Qt::Key_D:
            mState.camera_offset_x -= 20.0f;
            break;
        case Qt::Key_Left:
            UpdateCurrentNodePosition(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            UpdateCurrentNodePosition(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            UpdateCurrentNodePosition(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            UpdateCurrentNodePosition(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            mUI.tree->ClearSelection();
            break;
        default:
            return false;
    }
    return true;
}

void EntityWidget::DisplayCurrentNodeProperties()
{
    SetValue(mUI.drawableItem, false);
    SetValue(mUI.rigidBodyItem, false);
    SetValue(mUI.dsMaterial, QString(""));
    SetValue(mUI.dsDrawable, QString(""));
    SetValue(mUI.dsLayer, 0);
    SetValue(mUI.dsRenderPass, game::DrawableItemClass::RenderPass::Draw);
    SetValue(mUI.dsRenderStyle, game::DrawableItemClass::RenderStyle::Solid);
    SetValue(mUI.dsAlpha, NormalizedFloat(1.0f));
    SetValue(mUI.rbFriction, 0.0f);
    SetValue(mUI.rbRestitution, 0.0f);
    SetValue(mUI.rbAngularDamping, 0.0f);
    SetValue(mUI.rbLinearDamping, 0.0f);
    SetValue(mUI.rbDensity, 0.0f);
    SetValue(mUI.rbIsBullet, false);
    SetValue(mUI.rbIsSensor, false);
    SetValue(mUI.rbIsEnabled, false);

    if (const auto* node = GetCurrentNode())
    {
        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetClassId());
        SetValue(mUI.nodeName, node->GetName());
        SetValue(mUI.nodeIsVisible, node->TestFlag(game::EntityNodeClass::Flags::VisibleInGame));
        SetValue(mUI.nodeTranslateX, translate.x);
        SetValue(mUI.nodeTranslateY, translate.y);
        SetValue(mUI.nodeSizeX, size.x);
        SetValue(mUI.nodeSizeY, size.y);
        SetValue(mUI.nodeScaleX, scale.x);
        SetValue(mUI.nodeScaleY, scale.y);
        SetValue(mUI.nodeRotation, qRadiansToDegrees(node->GetRotation()));
        if (const auto* item = node->GetDrawable())
        {
            const auto& material = mState.workspace->MapMaterialIdToName(item->GetMaterialId());
            const auto& drawable = mState.workspace->MapDrawableIdToName(item->GetDrawableId());
            SetValue(mUI.drawableItem, true);
            SetValue(mUI.dsMaterial, material);
            SetValue(mUI.dsDrawable, drawable);
            SetValue(mUI.dsRenderPass, item->GetRenderPass());
            SetValue(mUI.dsRenderStyle, item->GetRenderStyle());
            SetValue(mUI.dsLayer, item->GetLayer());
            SetValue(mUI.dsLineWidth, item->GetLineWidth());
            SetValue(mUI.dsAlpha, NormalizedFloat(item->GetAlpha()));
            SetValue(mUI.dsUpdateDrawable, item->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable));
            SetValue(mUI.dsUpdateMaterial, item->TestFlag(game::DrawableItemClass::Flags::UpdateMaterial));
            SetValue(mUI.dsRestartDrawable, item->TestFlag(game::DrawableItemClass::Flags::RestartDrawable));
            SetValue(mUI.dsOverrideAlpha, item->TestFlag(game::DrawableItemClass::Flags::OverrideAlpha));
        }
        if (const auto* body = node->GetRigidBody())
        {
            SetValue(mUI.rigidBodyItem, true);
            SetValue(mUI.rbSimulation, body->GetSimulation());
            SetValue(mUI.rbShape, body->GetCollisionShape());
            SetValue(mUI.rbFriction, body->GetFriction());
            SetValue(mUI.rbRestitution, body->GetRestitution());
            SetValue(mUI.rbAngularDamping, body->GetAngularDamping());
            SetValue(mUI.rbLinearDamping, body->GetLinearDamping());
            SetValue(mUI.rbDensity, body->GetDensity());
            SetValue(mUI.rbIsBullet, body->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
            SetValue(mUI.rbIsSensor, body->TestFlag(game::RigidBodyItemClass::Flags::Sensor));
            SetValue(mUI.rbIsEnabled, body->TestFlag(game::RigidBodyItemClass::Flags::Enabled));
            if (body->GetCollisionShape() == game::RigidBodyItemClass::CollisionShape::Polygon)
            {
                SetEnabled(mUI.rbPolygon, true);
                SetValue(mUI.rbPolygon, mState.workspace->MapDrawableIdToName(body->GetPolygonShapeId()));
            }
            else
            {
                SetEnabled(mUI.rbPolygon, false);
                SetValue(mUI.rbPolygon, QString(""));
            }
        }
    }
}

void EntityWidget::DisplayCurrentCameraLocation()
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto dist_x = mState.camera_offset_x - (width / 2.0f);
    const auto dist_y = mState.camera_offset_y - (height / 2.0f);
    SetValue(mUI.translateX, dist_x);
    SetValue(mUI.translateY, dist_y);
}

void EntityWidget::UncheckPlacementActions()
{
    mUI.actionNewRect->setChecked(false);
    mUI.actionNewCircle->setChecked(false);
    mUI.actionNewIsoscelesTriangle->setChecked(false);
    mUI.actionNewRightTriangle->setChecked(false);
    mUI.actionNewRoundRect->setChecked(false);
    mUI.actionNewTrapezoid->setChecked(false);
    mUI.actionNewParallelogram->setChecked(false);
    mUI.actionNewCapsule->setChecked(false);
}

void EntityWidget::UpdateCurrentNodePosition(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
    }
}

void EntityWidget::UpdateCurrentNodeAlpha()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* item = node->GetDrawable())
        {
            const float value = mUI.dsAlpha->value();
            const float max   = mUI.dsAlpha->maximum();
            const float alpha = value / max;
            const bool checked = GetValue(mUI.dsOverrideAlpha);
            item->SetFlag(game::DrawableItemClass::Flags::OverrideAlpha, checked);
            item->SetAlpha(alpha);
            const auto &material = mState.workspace->FindMaterialClass(item->GetMaterialId());

            if (!checked || !material)
                return;

            const bool has_alpha_blending = material->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Transparent;
            if (has_alpha_blending)
                return;

            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Warning);
            msg.setText(tr("The current material doesn't enable transparency. Setting alpha will have no effect."));
            msg.exec();
        }
    }
}

void EntityWidget::RebuildMenus()
{
    // rebuild the drawable menus for custom shapes
    // and particle systems.
    mParticleSystems->clear();
    mCustomShapes->clear();
    for (size_t i = 0; i < mState.workspace->GetNumResources(); ++i)
    {
        const auto &resource = mState.workspace->GetResource(i);
        const auto &name = resource.GetName();
        if (resource.GetType() == app::Resource::Type::ParticleSystem)
        {
            QAction *action = mParticleSystems->addAction(name);
            connect(action, &QAction::triggered, this, &EntityWidget::PlaceNewParticleSystem);
        }
        else if (resource.GetType() == app::Resource::Type::CustomShape)
        {
            QAction *action = mCustomShapes->addAction(name);
            connect(action, &QAction::triggered,this, &EntityWidget::PlaceNewCustomShape);
        }
    }
}

void EntityWidget::RebuildCombos()
{
    SetList(mUI.dsMaterial, mState.workspace->ListAllMaterials());
    SetList(mUI.dsDrawable, mState.workspace->ListAllDrawables());

    QStringList polygons;
    // for the rigid body we need to list the polygonal (custom) shape
    // objects. (note that it's actually possible that these would be concave
    // but this case isn't currently supported)
    for (size_t i=0; i<mState.workspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mState.workspace->GetUserDefinedResource(i);
        if (res.GetType() != app::Resource::Type::CustomShape)
            continue;
        polygons.append(res.GetName());
    }
    SetList(mUI.rbPolygon, polygons);
}

void EntityWidget::UpdateDeletedResourceReferences()
{
    for (size_t i=0; i<mState.entity.GetNumNodes(); ++i)
    {
        auto& node = mState.entity.GetNode(i);
        if (!node.HasDrawable())
            continue;
        auto* draw = node.GetDrawable();
        const auto drawable = draw->GetDrawableId();
        const auto material = draw->GetMaterialId();
        if (!mState.workspace->IsValidMaterial(material))
        {
            WARN("Entity node '%1' uses material that is no longer available.", node.GetName());
            draw->ResetMaterial();
            draw->SetMaterialId("_checkerboard");
        }
        if (!mState.workspace->IsValidDrawable(drawable))
        {
            WARN("Entity node '%1' uses drawable that is no longer available.", node.GetName());
            draw->ResetDrawable();
            draw->SetDrawableId("_rect");
        }
    }
}

game::EntityNodeClass* EntityWidget::GetCurrentNode()
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

} // bui
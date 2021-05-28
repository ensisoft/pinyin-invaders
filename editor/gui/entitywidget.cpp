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
#  include <QFile>
#  include <QFileInfo>
#  include <QFileDialog>
#  include <QTextStream>
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
#include "editor/gui/animationtrackwidget.h"
#include "editor/gui/scriptwidget.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgscriptvar.h"
#include "editor/gui/dlgmaterial.h"
#include "editor/gui/clipboard.h"
#include "base/assert.h"
#include "base/format.h"
#include "base/math.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "engine/treeop.h"

namespace gui
{

class EntityWidget::ScriptVarModel : public QAbstractTableModel
{
public:
    ScriptVarModel(EntityWidget::State& state) : mState(state)
    {}
    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        const auto& var = mState.entity->GetScriptVar(index.row());
        if (role == Qt::DisplayRole)
        {
            switch (index.column()) {
                case 0: return app::toString(var.GetType());
                case 1: return app::FromUtf8(var.GetName());
                case 2: return GetScriptVarData(var);
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            switch (section) {
                case 0: return "Type";
                case 1: return "Name";
                case 2: return "Data";
                default: BUG("Unknown script variable data index.");
            }
        }
        return QVariant();
    }
    virtual int rowCount(const QModelIndex&) const override
    {
        return static_cast<int>(mState.entity->GetNumScriptVars());
    }
    virtual int columnCount(const QModelIndex&) const override
    {
        return 3;
    }
    void AddVariable(game::ScriptVar&& var)
    {
        const auto count = static_cast<int>(mState.entity->GetNumScriptVars());
        beginInsertRows(QModelIndex(), count, count);
        mState.entity->AddScriptVar(std::move(var));
        endInsertRows();
    }
    void EditVariable(size_t row, game::ScriptVar&& var)
    {
        mState.entity->SetScriptVar(row, std::move(var));
        emit dataChanged(index(row, 0), index(row, 3));
    }
    void DeleteVariable(size_t row)
    {
        beginRemoveRows(QModelIndex(), row, row);
        mState.entity->DeleteScriptVar(row);
        endRemoveRows();
    }
    void Reset()
    {
        beginResetModel();
        endResetModel();
    }

private:
    static QVariant GetScriptVarData(const game::ScriptVar& var)
    {
        switch (var.GetType())
        {
            case game::ScriptVar::Type::Boolean:
                return var.GetValue<bool>();
            case game::ScriptVar::Type::String:
                return app::FromUtf8(var.GetValue<std::string>());
            case game::ScriptVar::Type::Float:
                return var.GetValue<float>();
            case game::ScriptVar::Type::Integer:
                return var.GetValue<int>();
            case game::ScriptVar::Type::Vec2: {
                const auto& val = var.GetValue<glm::vec2>();
                return QString("%1,%2").arg(QString::number(val.x, 'f', 2))
                        .arg(QString::number(val.y, 'f', 2));
            }
        }
        BUG("Unknown ScriptVar type.");
        return QVariant();
    }
private:
    EntityWidget::State& mState;
};

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
            return true;

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
        auto* child = mState.entity->AddNode(std::move(node));
        mState.entity->LinkChild(nullptr, child);
        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(child->GetId()));
        DEBUG("Added new shape '%1'", name);
        return true;
    }
    bool CheckNameAvailability(const std::string& name) const
    {
        for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
        {
            const auto& node = mState.entity->GetNode(i);
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


EntityWidget::EntityWidget(app::Workspace* workspace) : mUndoStack(3)
{
    DEBUG("Create EntityWidget");

    mState.entity = std::make_shared<game::EntityClass>();
    mState.entity->SetName("My Entity");

    mRenderTree.reset(new TreeModel(*mState.entity));
    mScriptVarModel.reset(new ScriptVarModel(mState));

    mUI.setupUi(this);
    mUI.scriptVarList->setModel(mScriptVarModel.get());
    QHeaderView* verticalHeader = mUI.scriptVarList->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mUI.widget->onZoomIn       = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomIn, this)); };
    mUI.widget->onZoomOut      = [this]() { MouseZoom(std::bind(&EntityWidget::ZoomOut, this)); };
    mUI.widget->onMouseMove    = std::bind(&EntityWidget::MouseMove, this, std::placeholders::_1);
    mUI.widget->onMousePress   = std::bind(&EntityWidget::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseRelease = std::bind(&EntityWidget::MouseRelease, this, std::placeholders::_1);
    mUI.widget->onKeyPress     = std::bind(&EntityWidget::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&EntityWidget::MouseDoubleClick, this, std::placeholders::_1);
    mUI.widget->onPaintScene   = std::bind(&EntityWidget::PaintScene, this, std::placeholders::_1,
                                           std::placeholders::_2);
    mUI.widget->onInitScene    = std::bind(&EntityWidget::InitScene, this, std::placeholders::_1,
                                           std::placeholders::_2);

    // create the menu for creating instances of user defined drawables
    // since there doesn't seem to be a way to do this in the designer.
    mParticleSystems = new QMenu(this);
    mParticleSystems->menuAction()->setIcon(QIcon("icons:particle.png"));
    mParticleSystems->menuAction()->setText("Particles");
    mCustomShapes = new QMenu(this);
    mCustomShapes->menuAction()->setIcon(QIcon("icons:polygon.png"));
    mCustomShapes->menuAction()->setText("Shapes");

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
    PopulateFromEnum<game::TextItemClass::VerticalTextAlign>(mUI.tiVAlign);
    PopulateFromEnum<game::TextItemClass::HorizontalTextAlign>(mUI.tiHAlign);

    PopulateFontNames(mUI.tiFontName);
    PopulateFontSizes(mUI.tiFontSize);

    SetValue(mUI.cmbGrid, GridDensity::Grid50x50);
    SetValue(mUI.entityName, mState.entity->GetName());
    SetValue(mUI.entityID,   mState.entity->GetId());
    SetValue(mUI.entityLifetime, mState.entity->GetLifetime());
    SetValue(mUI.chkEntityLifetime, mState.entity->TestFlag(game::EntityClass::Flags::LimitLifetime));
    SetValue(mUI.chkKillAtLifetime, mState.entity->TestFlag(game::EntityClass::Flags::KillAtLifetime));
    setWindowTitle("My Entity");

    RebuildMenus();
    RebuildCombos();

    RegisterEntityWidget(this);
    DisplayCurrentNodeProperties();
}

EntityWidget::EntityWidget(app::Workspace* workspace, const app::Resource& resource)
  : EntityWidget(workspace)
{
    DEBUG("Editing entity '%1'", resource.GetName());
    const game::EntityClass* content = nullptr;
    resource.GetContent(&content);

    mState.entity = std::make_shared<game::EntityClass>(*content);
    mOriginalHash = mState.entity->GetHash();
    mScriptVarModel->Reset();

    const auto vars = mState.entity->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);

    SetValue(mUI.entityName, content->GetName());
    SetValue(mUI.entityID, content->GetId());
    SetValue(mUI.scriptFile, ListItemId(content->GetScriptFileId()));
    SetValue(mUI.entityLifetime, content->GetLifetime());
    SetValue(mUI.chkEntityLifetime, mState.entity->TestFlag(game::EntityClass::Flags::LimitLifetime));
    SetValue(mUI.chkKillAtLifetime, mState.entity->TestFlag(game::EntityClass::Flags::KillAtLifetime));
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "grid", mUI.cmbGrid);
    GetUserProperty(resource, "snap", mUI.chkSnap);
    GetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    GetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    GetUserProperty(resource, "widget", mUI.widget);
    GetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    GetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    GetUserProperty(resource, "camera_rotation", mUI.rotation);
    mCameraWasLoaded = GetUserProperty(resource, "camera_offset_x", &mState.camera_offset_x) &&
                       GetUserProperty(resource, "camera_offset_y", &mState.camera_offset_y);

    // load per track resource properties.
    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        const auto& track = mState.entity->GetAnimationTrack(i);
        const auto& Id = track.GetId();
        QVariantMap properties;
        GetProperty(resource, "track_" + app::FromUtf8(Id), &properties);
        mTrackProperties[Id] = properties;
    }

    setWindowTitle(resource.GetName());

    UpdateDeletedResourceReferences();

    mRenderTree.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
}

EntityWidget::~EntityWidget()
{
    DEBUG("Destroy EntityWidget");

    DeleteEntityWidget(this);
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
    bar.addAction(mUI.actionNewSemiCircle);
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
    menu.addAction(mUI.actionNewSemiCircle);
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
    settings.saveWidget("Entity", mUI.entityName);
    settings.saveWidget("Entity", mUI.entityID);
    settings.saveWidget("Entity", mUI.entityLifetime);
    settings.saveWidget("Entity", mUI.chkEntityLifetime);
    settings.saveWidget("Entity", mUI.chkKillAtLifetime);
    settings.saveWidget("Entity", mUI.scaleX);
    settings.saveWidget("Entity", mUI.scaleY);
    settings.saveWidget("Entity", mUI.rotation);
    settings.saveWidget("Entity", mUI.chkShowOrigin);
    settings.saveWidget("Entity", mUI.chkShowGrid);
    settings.saveWidget("Entity", mUI.chkSnap);
    settings.saveWidget("Entity", mUI.cmbGrid);
    settings.saveWidget("Entity", mUI.zoom);
    settings.saveWidget("Entity", mUI.widget);
    settings.setValue("Entity", "camera_offset_x", mState.camera_offset_x);
    settings.setValue("Entity", "camera_offset_y", mState.camera_offset_y);

    for (const auto& p : mTrackProperties)
    {
        settings.setValue("Entity", app::FromUtf8(p.first), p.second);
    }

    // the entity can already serialize into JSON.
    // so let's use the JSON serialization in the entity
    // and then convert that into base64 string which we can
    // stick in the settings data stream.
    const auto& json = mState.entity->ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Entity", "content", base64);
    return true;
}
bool EntityWidget::LoadState(const Settings& settings)
{
    settings.loadWidget("Entity", mUI.entityName);
    settings.loadWidget("Entity", mUI.entityID);
    settings.loadWidget("Entity", mUI.entityLifetime);
    settings.loadWidget("Entity", mUI.chkEntityLifetime);
    settings.loadWidget("Entity", mUI.chkKillAtLifetime);
    settings.loadWidget("Entity", mUI.scaleX);
    settings.loadWidget("Entity", mUI.scaleY);
    settings.loadWidget("Entity", mUI.rotation);
    settings.loadWidget("Entity", mUI.chkShowOrigin);
    settings.loadWidget("Entity", mUI.chkShowGrid);
    settings.loadWidget("Entity", mUI.chkSnap);
    settings.loadWidget("Entity", mUI.cmbGrid);
    settings.loadWidget("Entity", mUI.zoom);
    settings.loadWidget("Entity", mUI.widget);
    settings.getValue("Entity", "camera_offset_x", &mState.camera_offset_x);
    settings.getValue("Entity", "camera_offset_y", &mState.camera_offset_y);
    setWindowTitle(mUI.entityName->text());

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
    auto klass = std::move(ret.value());
    auto hash  = klass.GetHash();
    mState.entity = FindSharedEntity(hash);
    if (!mState.entity)
    {
        mState.entity = std::make_shared<game::EntityClass>(std::move(klass));
        ShareEntity(mState.entity);
    }

    mOriginalHash = mState.entity->GetHash();

    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        const auto& track = mState.entity->GetAnimationTrack(i);
        QVariantMap properties;
        settings.getValue("Entity", app::FromUtf8(track.GetId()), &properties);
        mTrackProperties[track.GetId()] = properties;
    }

    UpdateDeletedResourceReferences();

    const auto vars = mState.entity->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
    mScriptVarModel->Reset();

    mRenderTree.reset(new TreeModel(*mState.entity));
    mUI.tree->SetModel(mRenderTree.get());
    mUI.tree->Rebuild();
    return true;
}

bool EntityWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanPaste:
            if (!mUI.widget->hasInputFocus())
                return false;
            else if (clipboard->IsEmpty())
                return false;
            else if (clipboard->GetType() != "application/json/entity")
                return false;
            return true;
        case Actions::CanCopy:
        case Actions::CanCut:
            if (!mUI.widget->hasInputFocus())
                return false;
            else if (!GetCurrentNode())
                return false;
            return true;
        case Actions::CanUndo:
            return mUndoStack.size() > 1;
        case Actions::CanZoomIn:
        {
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        }
            break;
        case Actions::CanZoomOut:
        {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        }
            break;
        case Actions::CanReloadShaders:
        case Actions::CanReloadTextures:
            return true;
    }
    return false;
}

void EntityWidget::Cut(Clipboard& clipboard)
{
    if (auto* node = GetCurrentNode())
    {
        const auto& tree = mState.entity->GetRenderTree();
        const auto& json = tree.ToJson([](const auto* node) {return node->ToJson(); }, node);
        clipboard.SetType("application/json/entity");
        clipboard.SetText(json.dump(2));

        NOTE("Copied JSON to application clipboard.");
        DEBUG("Copied entity node '%1' ('%2') to the clipboard.", node->GetId(), node->GetName());

        mState.entity->DeleteNode(node);
        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
    }
}
void EntityWidget::Copy(Clipboard& clipboard) const
{
    if (const auto* node = GetCurrentNode())
    {
        const auto& tree = mState.entity->GetRenderTree();
        const auto& json = tree.ToJson([](const auto* node) {return node->ToJson(); }, node);
        clipboard.SetType("application/json/entity");
        clipboard.SetText(json.dump(2));

        NOTE("Copied JSON to application clipboard.");
        DEBUG("Copied entity node '%1' ('%2') to the clipboard.", node->GetId(), node->GetName());
    }
}
void EntityWidget::Paste(const Clipboard& clipboard)
{
    if (!mUI.widget->hasInputFocus())
        return;

    if (clipboard.GetType() != "application/json/entity")
    {
        NOTE("No entity JSON data found in clipboard.");
        return;
    }

    const auto& str = clipboard.GetText();
    auto [success, json, _] = base::JsonParse(str.begin(), str.end());
    if (!success)
    {
        NOTE("Clipboard JSON parse failed.");
        return;
    }

    // use a temporary vector in case there's a problem
    std::vector<std::unique_ptr<game::EntityNodeClass>> nodes;
    bool error = false;
    game::EntityClass::RenderTree tree;
    tree.FromJson(json, [&nodes, &error](const nlohmann::json& json) {
        auto ret = game::EntityNodeClass::FromJson(json);
        if (ret.has_value()) {
            auto node = std::make_unique<game::EntityNodeClass>(ret->Clone());
            node->SetName(base::FormatString("Copy of %1", ret->GetName()));
            nodes.push_back(std::move(node));
            return nodes.back().get();
        }
        error = true;
        return (game::EntityNodeClass*)nullptr;
    });
    if (error || nodes.empty())
    {
        NOTE("No render tree JSON found.");
        return;
    }

    // if the mouse pointer is not within the widget then adjust
    // the paste location to the center of the widget.
    QPoint mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    if (mickey.x() < 0 || mickey.x() > mUI.widget->width() ||
        mickey.y() < 0 || mickey.y() > mUI.widget->height())
        mickey = QPoint(mUI.widget->width() * 0.5, mUI.widget->height() * 0.5);

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate(qDegreesToRadians((float)GetValue(mUI.rotation)));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);
    const auto& view_to_scene   = glm::inverse(view.GetAsMatrix());
    const auto& mouse_pos_view  = ToVec4(mickey);
    const auto& mouse_pos_scene = view_to_scene * mouse_pos_view;

    auto* paste_root = nodes[0].get();
    paste_root->SetTranslation(glm::vec2(mouse_pos_scene.x, mouse_pos_scene.y));
    tree.LinkChild(nullptr, paste_root);

    // if we got this far, nodes should contain the nodes to be added
    // into the scene and tree should contain their hierarchy.
    for (auto& node : nodes)
    {
        // moving the unique ptr means that node address stays the same
        // thus the tree is still valid!
        mState.entity->AddNode(std::move(node));
    }
    nodes.clear();
    // walk the tree and link the nodes into the scene.
    tree.PreOrderTraverseForEach([&nodes, this, &tree](game::EntityNodeClass* node) {
        if (node == nullptr)
            return;
        auto* parent = tree.GetParent(node);
        mState.entity->LinkChild(parent, node);
    });

    mUI.tree->Rebuild();
    mUI.tree->SelectItemById(app::FromUtf8(paste_root->GetId()));
}

void EntityWidget::Save()
{
    on_actionSave_triggered();
}

void EntityWidget::Undo()
{
    if (mUndoStack.size() <= 1)
    {
        NOTE("No undo available.");
        return;
    }

    // if the timer has run the top of the undo stack
    // is the same copy as the actual scene object.
    if (mUndoStack.back().GetHash() == mState.entity->GetHash())
        mUndoStack.pop_back();

    // todo: how to deal with entity being changed when the
    // animation track widget is open?

    *mState.entity = mUndoStack.back();
    mState.view->Rebuild();
    mUndoStack.pop_back();
    mScriptVarModel->Reset();
    DisplayCurrentNodeProperties();
    NOTE("Undo!");
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
        mState.renderer.Update(*mState.entity, mEntityTime, secs);
        mEntityTime += secs;
    }
    mCurrentTime += secs;
}
void EntityWidget::Render()
{
    mUI.widget->triggerPaint();
}

bool EntityWidget::HasUnsavedChanges() const
{
    if (!mOriginalHash)
        return false;
    const auto hash = mState.entity->GetHash();
    return hash != mOriginalHash;
}

bool EntityWidget::ConfirmClose()
{
    const auto hash = mState.entity->GetHash();
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
    // don't take an undo snapshot while the mouse tool is in
    // action.
    if (mCurrentTool)
        return;
    // don't take an undo snapshot while the node name is being
    // edited.
    if (mUI.nodeName->hasFocus())
        return;
    // don't take undo snapshot while the node alpha is being slid
    if (mUI.dsAlpha->isSliderDown())
        return;
    // don't take undo snapshot while continuous edits to text props
    if (mUI.tiTextColor->isDialogOpen() || mUI.tiText->hasFocus())
        return;

    if (mUndoStack.empty())
    {
        mUndoStack.push_back(*mState.entity);
    }

    const auto curr_hash = mState.entity->GetHash();
    const auto undo_hash = mUndoStack.back().GetHash();
    if (curr_hash != undo_hash)
    {
        mUndoStack.push_back(*mState.entity);
        DEBUG("Created undo copy. stack size: %1", mUndoStack.size());
    }

    bool changes = false;
    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        if (i >= mUI.trackList->count())
        {
            changes = true;
            break;
        }
        const auto& track = mState.entity->GetAnimationTrack(i);
        const auto& name  = app::FromUtf8(track.GetName());
        const auto& id    = app::FromUtf8(track.GetId());
        const auto* item  = mUI.trackList->item(i);
        if (item->text() != name || item->data(Qt::UserRole).toString() != id)
        {
            changes = true;
            break;
        }
    }
    if (!changes)
        return;

    auto selected = mUI.trackList->selectedItems();
    std::unordered_set<std::string> selected_item_ids;
    for (const auto* item : selected)
        selected_item_ids.insert(app::ToUtf8(item->data(Qt::UserRole).toString()));

    QSignalBlocker s(mUI.idleTrack);
    mUI.trackList->clear();
    mUI.idleTrack->clear();
    int selected_idle_track_index = -1;
    SetEnabled(mUI.btnDeleteTrack, false);
    SetEnabled(mUI.btnEditTrack, false);
    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        const auto& track = mState.entity->GetAnimationTrack(i);
        const auto& name  = app::FromUtf8(track.GetName());
        const auto& id    = app::FromUtf8(track.GetId());
        QListWidgetItem* item = new QListWidgetItem;
        item->setText(name);
        item->setData(Qt::UserRole, id);
        item->setIcon(QIcon("icons:animation_track.png"));
        mUI.trackList->addItem(item);
        mUI.idleTrack->addItem(name, QVariant(id)); // track id as user data
        if (selected_item_ids.find(track.GetId()) != selected_item_ids.end())
            item->setSelected(true);
        if (mState.entity->HasIdleTrack())
        {
            if (id == app::FromUtf8(mState.entity->GetIdleTrackId()))
                selected_idle_track_index = i;
        }
    }
    SetValue(mUI.idleTrack, selected_idle_track_index);
}

bool EntityWidget::GetStats(Stats* stats) const
{
    stats->time  = mEntityTime;
    stats->fps   = mUI.widget->getCurrentFPS();
    stats->vsync = mUI.widget->haveVSYNC();
    return true;
}

void EntityWidget::SaveAnimationTrack(const game::AnimationTrackClass& track, const QVariantMap& properties)
{
    // keep track of the associated track properties 
    // separately. these only pertain to the UI and are not
    // used by the track/animation system itself.
    mTrackProperties[track.GetId()] = properties;

    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        auto& other = mState.entity->GetAnimationTrack(i);
        if (other.GetId() != track.GetId())
            continue;

        // copy it over.
        other = track;
        INFO("Saved animation track '%1'", track.GetName());
        NOTE("Saved animation track '%1'", track.GetName());
        return;
    }
    // add a copy
    mState.entity->AddAnimationTrack(track);
    INFO("Saved animation track '%1'", track.GetName());
    NOTE("Saved animation track '%1'", track.GetName());
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
}
void EntityWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.entityName))
        return;
    const QString& name = GetValue(mUI.entityName);
    mState.entity->SetName(GetValue(mUI.entityName));
    app::EntityResource resource(*mState.entity, name);
    SetUserProperty(resource, "camera_offset_x", mState.camera_offset_x);
    SetUserProperty(resource, "camera_offset_y", mState.camera_offset_y);
    SetUserProperty(resource, "camera_scale_x", mUI.scaleX);
    SetUserProperty(resource, "camera_scale_y", mUI.scaleY);
    SetUserProperty(resource, "camera_rotation", mUI.rotation);
    SetUserProperty(resource, "zoom", mUI.zoom);
    SetUserProperty(resource, "grid", mUI.cmbGrid);
    SetUserProperty(resource, "snap", mUI.chkSnap);
    SetUserProperty(resource, "show_origin", mUI.chkShowOrigin);
    SetUserProperty(resource, "show_grid", mUI.chkShowGrid);
    SetUserProperty(resource, "widget", mUI.widget);

    // save the track properties.
    for (auto p : mTrackProperties)
    {
        SetProperty(resource, "track_" + app::FromUtf8(p.first), p.second);
    }

    mState.workspace->SaveResource(resource);
    mOriginalHash = mState.entity->GetHash();

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

void EntityWidget::on_actionNewSemiCircle_triggered()
{
    mCurrentTool.reset(new PlaceShapeTool(mState, "Checkerboard", "SemiCircle"));

    UncheckPlacementActions();
    mUI.actionNewSemiCircle->setChecked(true);
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
        mState.entity->DeleteNode(node);

        mUI.tree->Rebuild();
        mUI.tree->ClearSelection();
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
        auto* dupe = mState.entity->DuplicateNode(node);
        // update the the translation for the parent of the new hierarchy
        // so that it's possible to tell it apart from the source of the copy.
        dupe->SetTranslation(node->GetTranslation() * 1.2f);

        mState.view->Rebuild();
        mState.view->SelectItemById(app::FromUtf8(dupe->GetId()));
    }
}

void EntityWidget::on_entityName_textChanged(const QString& text)
{
    mState.entity->SetName(GetValue(mUI.entityName));
}

void EntityWidget::on_entityLifetime_valueChanged(double value)
{
    mState.entity->SetLifetime(GetValue(mUI.entityLifetime));
}

void EntityWidget::on_chkKillAtLifetime_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::KillAtLifetime, GetValue(mUI.chkKillAtLifetime));
}

void EntityWidget::on_chkEntityLifetime_stateChanged(int)
{
    mState.entity->SetFlag(game::EntityClass::Flags::LimitLifetime, GetValue(mUI.chkEntityLifetime));
}

void EntityWidget::on_btnAddIdleTrack_clicked()
{
    // todo:
}

void EntityWidget::on_btnResetIdleTrack_clicked()
{
    mState.entity->ResetIdleTrack();
    SetValue(mUI.idleTrack, -1);
}

void EntityWidget::on_btnAddScript_clicked()
{
    app::Script script;
    // use the script ID as the file name so that we can
    // avoid naming clashes and always find the correct lua
    // file even if the entity is later renamed.
    const auto& filename = app::FromUtf8(script.GetId());
    const auto& fileuri  = QString("ws://lua/%1.lua").arg(filename);
    const auto& filepath = mState.workspace->MapFileToFilesystem(fileuri);
    const auto& name = GetValue(mUI.entityName);
    const QFileInfo info(filepath);
    if (info.exists())
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(tr("File already exists"));
        msg.setText(tr("Overwrite existing script file?\n%1").arg(filepath));
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        if (msg.exec() == QMessageBox::Cancel)
            return;
    }

    QFile io;
    io.setFileName(filepath);
    if (!io.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        ERROR("Failed to open '%1' for writing (%2)", filepath, io.error());
        ERROR(io.errorString());
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle(tr("Error Occurred"));
        msg.setText(tr("There was a problem creating the script file.\n%1").arg(io.errorString()));
        msg.setStandardButtons(QMessageBox::Ok);
        return;
    }
    QString var = name;
    var.replace(' ', '_');
    var = var.toLower();

    QTextStream stream(&io);
    stream.setCodec("UTF-8");
    stream << QString("-- Entity '%1' script.\n\n").arg(name);
    stream << QString("-- This script will be called for every instance of '%1'\n"
                      "-- in the scene during gameplay.\n").arg(name);
    stream << "-- You're free to delete functions you don't need.\n\n";
    stream << "-- Called when the game play begins for a scene.\n";
    stream << QString("function BeginPlay(%1, scene)\n\nend\n\n").arg(var);
    stream << "-- Called on every low frequency game tick.\n";
    stream << QString("function Tick(%1, wall_time, tick_time, dt)\n\nend\n\n").arg(var);
    stream << "-- Called on every iteration of game loop.\n";
    stream << QString("function Update(%1, wall_time, game_time, dt)\n\nend\n\n").arg(var);
    stream << "-- Called on collision events with other objects.\n";
    stream << QString("function OnBeginContact(%1, node, other, other_node)\nend\n\n").arg(var);
    stream << "-- Called on collision events with other objects.\n";
    stream << QString("function OnEndContact(%1, node, other, other_node)\nend\n\n").arg(var);

    io.flush();
    io.close();

    script.SetFileURI(app::ToUtf8(fileuri));
    app::ScriptResource resource(script, name);
    mState.workspace->SaveResource(resource);
    mState.entity->SetSriptFileId(script.GetId());

    ScriptWidget* widget = new ScriptWidget(mState.workspace, resource);
    emit OpenNewWidget(widget);

    SetValue(mUI.scriptFile, ListItemId(script.GetId()));
}

void EntityWidget::on_btnResetScript_clicked()
{
    mState.entity->ResetScriptFile();
    SetValue(mUI.scriptFile, -1);
}

void EntityWidget::on_btnViewPlus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value + 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void EntityWidget::on_btnViewMinus90_clicked()
{
    const float value = GetValue(mUI.rotation);
    mUI.rotation->setValue(math::clamp(-180.0f, 180.0f, value - 90.0f));
    mViewTransformRotation  = value;
    mViewTransformStartTime = mCurrentTime;
}

void EntityWidget::on_btnResetTransform_clicked()
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

void EntityWidget::on_btnNewTrack_clicked()
{
    // sharing the animation class object with the new animation
    // track widget.
    AnimationTrackWidget* widget = new AnimationTrackWidget(mState.workspace, mState.entity);
    widget->SetZoom(GetValue(mUI.zoom));
    widget->SetShowGrid(GetValue(mUI.chkShowGrid));
    widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
    widget->SetSnapGrid(GetValue(mUI.chkSnap));
    widget->SetGrid(GetValue(mUI.cmbGrid));
    emit OpenNewWidget(widget);
}
void EntityWidget::on_btnEditTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    QString id = item->data(Qt::UserRole).toString();

    for (size_t i=0; i<mState.entity->GetNumTracks(); ++i)
    {
        const auto& klass = mState.entity->GetAnimationTrack(i);
        if (klass.GetId() != app::ToUtf8(id))
            continue;
        auto it = mTrackProperties.find(klass.GetId());
        ASSERT(it != mTrackProperties.end());
        const auto& properties = (*it).second;
        AnimationTrackWidget* widget = new AnimationTrackWidget(mState.workspace, mState.entity, klass, properties);
        widget->SetZoom(GetValue(mUI.zoom));
        widget->SetShowGrid(GetValue(mUI.chkShowGrid));
        widget->SetShowOrigin(GetValue(mUI.chkShowOrigin));
        widget->SetSnapGrid(GetValue(mUI.chkSnap));
        widget->SetGrid(GetValue(mUI.cmbGrid));
        emit OpenNewWidget(widget);
    }
}
void EntityWidget::on_btnDeleteTrack_clicked()
{
    auto items = mUI.trackList->selectedItems();
    if (items.isEmpty())
        return;
    QListWidgetItem* item = items[0];
    const auto& trackId = app::ToUtf8(item->data(Qt::UserRole).toString());

    if (mState.entity->HasIdleTrack())
    {
        if (mState.entity->GetIdleTrackId() == trackId)
        {
            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msg.setIcon(QMessageBox::Question);
            msg.setText(tr("The selected track is the current entity idle track.\n"
                           "Are you sure you want to delete it?"));
            if (msg.exec() == QMessageBox::No)
                return;
            mState.entity->ResetIdleTrack();
            SetValue(mUI.idleTrack, -1);
        }
    }
    mState.entity->DeleteAnimationTrackById(trackId);
    // this will remove it from the widget.
    delete item;
    // delete the associated properties.
    auto it = mTrackProperties.find(trackId);
    ASSERT(it != mTrackProperties.end());
    mTrackProperties.erase(it);
}

void EntityWidget::on_btnNewScriptVar_clicked()
{
    game::ScriptVar var("My_Var", std::string(""));
    DlgScriptVar dlg(this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->AddVariable(std::move(var));
    SetEnabled(mUI.btnEditScriptVar, true);
    SetEnabled(mUI.btnDeleteScriptVar, true);
}
void EntityWidget::on_btnEditScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    game::ScriptVar var = mState.entity->GetScriptVar(index.row());
    DlgScriptVar dlg(this, var);
    if (dlg.exec() == QDialog::Rejected)
        return;

    mScriptVarModel->EditVariable(index.row(), std::move(var));
}

void EntityWidget::on_btnDeleteScriptVar_clicked()
{
    auto items = mUI.scriptVarList->selectionModel()->selectedRows();
    if (items.isEmpty())
        return;

    // single selection for now.
    const auto index = items[0];
    mScriptVarModel->DeleteVariable(index.row());
    const auto vars = mState.entity->GetNumScriptVars();
    SetEnabled(mUI.btnEditScriptVar, vars > 0);
    SetEnabled(mUI.btnDeleteScriptVar, vars > 0);
}

void EntityWidget::on_btnSelectMaterial_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* drawable = node->GetDrawable())
        {
            QString material = app::FromUtf8(drawable->GetMaterialId());
            DlgMaterial dlg(this, mState.workspace, material);
            if (dlg.exec() == QDialog::Rejected)
                return;
            drawable->SetMaterialId(app::ToUtf8(dlg.GetSelectedMaterialId()));
        }
    }
}

void EntityWidget::on_trackList_itemSelectionChanged()
{
    auto list = mUI.trackList->selectedItems();
    mUI.btnEditTrack->setEnabled(!list.isEmpty());
    mUI.btnDeleteTrack->setEnabled(!list.isEmpty());
}

void EntityWidget::on_idleTrack_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetIdleTrack();
        return;
    }
    const auto id   = mUI.idleTrack->currentData().toString();
    const auto name = mUI.idleTrack->currentText();
    mState.entity->SetIdleTrackId(app::ToUtf8(id));
    DEBUG("Entity idle track set to '%1' ('%2')", name, id);
}

void EntityWidget::on_scriptFile_currentIndexChanged(int index)
{
    if (index == -1)
    {
        mState.entity->ResetIdleTrack();
        return;
    }
    const auto id   = mUI.scriptFile->currentData().toString();
    const auto name = mUI.scriptFile->currentText();
    mState.entity->SetSriptFileId(app::ToUtf8(id));
    DEBUG("Entity script file set to '%1' ('%2')", name, id);
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
    UpdateCurrentNodeProperties();
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
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsMaterial_currentIndexChanged(const QString& name)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRenderPass_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRenderStyle_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLayer_valueChanged(int value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsLineWidth_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsTimeScale_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsUpdateDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsUpdateMaterial_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsRestartDrawable_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_dsOverrideAlpha_stateChanged(int)
{
    UpdateCurrentNodeProperties();

    if (auto* node = GetCurrentNode())
    {
        if (const auto* item = node->GetDrawable())
        {
            const auto& material = mState.workspace->FindMaterialClassById(item->GetMaterialId());
            if (!material || !item->TestFlag(game::DrawableItemClass::Flags::OverrideAlpha))
                return;

            const bool has_alpha_blending = material->GetSurfaceType() == gfx::MaterialClass::SurfaceType::Transparent;
            if (has_alpha_blending)
                return;

            QMessageBox msg(this);
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Warning);
            msg.setText(tr("The current material doesn't enable transparency."
                           "Setting alpha will have no effect."));
            msg.exec();
        }
    }
}
void EntityWidget::on_dsFlipVertically_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_dsAlpha_valueChanged()
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbSimulation_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbShape_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_rbPolygon_currentIndexChanged(const QString&)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbFriction_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbRestitution_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbAngularDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbLinearDamping_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDensity_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbLinearVeloX_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbLinearVeloY_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbAngularVelo_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbIsBullet_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsSensor_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbIsEnabled_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_rbCanSleep_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_rbDiscardRotation_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontName_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiFontSize_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiHAlign_currentIndexChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiTextColor_colorChanged(QColor color)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLineHeight_valueChanged(double value)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiLayer_valueChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiText_textChanged()
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiVisible_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiUnderline_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}
void EntityWidget::on_tiBlink_stateChanged(int)
{
    UpdateCurrentNodeProperties();
}

void EntityWidget::on_btnSelectFont_clicked()
{
    if (auto* node = GetCurrentNode())
    {
        if (auto* text = node->GetTextItem())
        {
            const auto& list = QFileDialog::getOpenFileNames(this ,
                tr("Select Font File") , "" , tr("Font (*.ttf *.otf)"));
            if (list.isEmpty())
                return;
            const auto& file = mState.workspace->MapFileToWorkspace(list[0]);
            SetValue(mUI.tiFontName , file);
            text->SetFontName(app::ToUtf8(file));
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
    auto* node = GetCurrentNode();
    if (!node)
        return;
    if (!on)
    {
        node->RemoveRigidBody();
        DEBUG("Removed rigid body from '%1'" , node->GetName());
    }
    else if (!node->HasRigidBody())
    {
        game::RigidBodyItemClass body;
        // try to see if we can figure out the right collision
        // box for this rigid body based on the drawable.
        if (auto* item = node->GetDrawable())
        {
            const auto& drawableId = item->GetDrawableId();
            if (drawableId == "_circle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Circle);
            else if (drawableId == "_parallelogram")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Parallelogram);
            else if (drawableId == "_rect" || drawableId == "_round_rect")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Box);
            else if (drawableId == "_isosceles_triangle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::IsoscelesTriangle);
            else if (drawableId == "_right_triangle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::RightTriangle);
            else if (drawableId == "_trapezoid")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Trapezoid);
            else if (drawableId == "_semi_circle")
                body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::SemiCircle);
            else if (auto klass = mState.workspace->FindDrawableClassById(drawableId)) {
                if (klass->GetType() == gfx::DrawableClass::Type::Polygon) {
                    body.SetPolygonShapeId(drawableId);
                    body.SetCollisionShape(game::RigidBodyItemClass::CollisionShape::Polygon);
                }
            }
        }
        node->SetRigidBody(body);
        DEBUG("Added rigid body to '%1'", node->GetName());
    }
    DisplayCurrentNodeProperties();
}

void EntityWidget::on_textItem_toggled(bool on)
{
    if (auto* node = GetCurrentNode())
    {
        if (on)
        {
            if (!node->HasTextItem())
            {
                game::TextItemClass text;
                text.SetFontSize(GetValue(mUI.tiFontSize));
                text.SetFontName(GetValue(mUI.tiFontName));
                node->SetTextItem(text);
                DEBUG("Added text item to '%1'", node->GetName());
            }
        }
        else
        {
            node->RemoveTextItem();
            DEBUG("Removed text item from '%1'" , node->GetName());
        }
        DisplayCurrentNodeProperties();
    }
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
    DisplayCurrentNodeProperties();
}
void EntityWidget::TreeDragEvent(TreeWidget::TreeItem* item, TreeWidget::TreeItem* target)
{
    auto& tree = mState.entity->GetRenderTree();
    auto* src_value = static_cast<game::EntityNodeClass*>(item->GetUserData());
    auto* dst_value = static_cast<game::EntityNodeClass*>(target->GetUserData());

    // check if we're trying to drag a parent onto its own child
    if (game::SearchChild(tree, dst_value, src_value))
        return;
    const bool retain_world_transform = true;
    mState.entity->ReparentChild(dst_value, src_value, retain_world_transform);
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
    const auto& resource = mState.workspace->GetResourceByName(drawable, app::Resource::Type::Shape);
    const auto& material = resource.GetProperty("material",  QString("Checkerboard"));
    mCurrentTool.reset(new PlaceShapeTool(mState, material, drawable));
}

void EntityWidget::InitScene(unsigned width, unsigned height)
{
    if (!mCameraWasLoaded)
    {
        // if the camera hasn't been loaded then compute now the
        // initial position for the camera.
        mState.camera_offset_x = mUI.widget->width()  * 0.5;
        mState.camera_offset_y = mUI.widget->height() * 0.5;
    }
    DisplayCurrentCameraLocation();
}

void EntityWidget::PaintScene(gfx::Painter& painter, double /*secs*/)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    const auto zoom   = (float)GetValue(mUI.zoom);
    const auto xs     = (float)GetValue(mUI.scaleX);
    const auto ys     = (float)GetValue(mUI.scaleY);
    const auto grid   = (GridDensity)GetValue(mUI.cmbGrid);
    const auto view_rotation_time = math::clamp(0.0, 1.0, mCurrentTime - mViewTransformStartTime);
    const auto view_rotation_angle = math::interpolate(mViewTransformRotation, (float)mUI.rotation->value(),
        view_rotation_time, math::Interpolation::Cosine);

    painter.SetViewport(0, 0, width, height);
    painter.SetPixelRatio(glm::vec2(xs*zoom, ys*zoom));

    // apply the view transformation. The view transformation is not part of the
    // animation per-se but it's the transformation that transforms the animation
    // and its components from the space of the animation to the global space.
    gfx::Transform view;
    view.Push();
    view.Scale(xs, ys);
    view.Scale(zoom, zoom);
    view.Rotate(qDegreesToRadians(view_rotation_angle));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    // render endless background grid.
    if (GetValue(mUI.chkShowGrid))
    {
        DrawCoordinateGrid(painter, view, grid, zoom, xs, ys, width, height);
    }

    DrawHook hook(GetCurrentNode(), mPlayState == PlayState::Playing);

    // begin the animation transformation space
    view.Push();
        mState.renderer.BeginFrame();
        mState.renderer.Draw(*mState.entity, painter, view, &hook);
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

void EntityWidget::MouseZoom(std::function<void(void)> zoom_function)
{
    // where's the mouse in the widget
    const auto& mickey = mUI.widget->mapFromGlobal(QCursor::pos());
    // can't use underMouse here because of the way the gfx widget
    // is constructed i.e QWindow and Widget as container
    if (mickey.x() < 0 || mickey.y() < 0 ||
        mickey.x() > mUI.widget->width() ||
        mickey.y() > mUI.widget->height())
        return;

    glm::vec4 mickey_pos_in_entity;
    glm::vec4 mickey_pos_in_widget;

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = glm::inverse(view.GetAsMatrix());
        mickey_pos_in_entity = mat * glm::vec4(mickey.x() , mickey.y() , 1.0f , 1.0f);
    }

    zoom_function();

    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        const auto& mat = view.GetAsMatrix();
        mickey_pos_in_widget = mat * mickey_pos_in_entity;
    }
    mState.camera_offset_x += (mickey.x() - mickey_pos_in_widget.x);
    mState.camera_offset_y += (mickey.y() - mickey_pos_in_widget.y);
    DisplayCurrentCameraLocation();
}

void EntityWidget::MouseMove(QMouseEvent* mickey)
{
    if (mCurrentTool)
    {
        gfx::Transform view;
        view.Scale(GetValue(mUI.scaleX) , GetValue(mUI.scaleY));
        view.Scale(GetValue(mUI.zoom) , GetValue(mUI.zoom));
        view.Rotate(qDegreesToRadians(mUI.rotation->value()));
        view.Translate(mState.camera_offset_x , mState.camera_offset_y);
        mCurrentTool->MouseMove(mickey , view);

        // update the properties that might have changed as the result of application
        // of the current tool.
        DisplayCurrentCameraLocation();
        DisplayCurrentNodeProperties();
    }
}
void EntityWidget::MousePress(QMouseEvent* mickey)
{
    const auto snap = (bool)GetValue(mUI.chkSnap);
    const auto grid = (GridDensity)GetValue(mUI.cmbGrid);
    const auto grid_size = (unsigned)grid;

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate((float)qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    if (!mCurrentTool)
    {
        auto [hitnode, hitpos] = SelectNode(mickey->pos(), view, *mState.entity, GetCurrentNode());
        if (hitnode)
        {
            view.Push(mState.entity->FindNodeTransform(hitnode));
                const auto mat = view.GetAsMatrix();
                glm::vec3 scale;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::quat orientation;
                glm::decompose(mat, scale, orientation, translation, skew,  perspective);
            view.Pop();
            
            const auto& size = hitnode->GetSize();
            const auto& box_size = glm::vec2(10.0f/scale.x, 10.0f/scale.y);
            // check if any particular special area of interest is being hit
            const bool bottom_right_hitbox_hit = hitpos.x >= size.x - box_size.x &&
                                                 hitpos.y >= size.y - box_size.y;
            const bool top_left_hitbox_hit = hitpos.x >= 0 && hitpos.x <= box_size.x &&
                                             hitpos.y >= 0 && hitpos.y <= box_size.y;
            if (bottom_right_hitbox_hit)
                mCurrentTool.reset(new ResizeRenderTreeNodeTool(*mState.entity, hitnode));
            else if (top_left_hitbox_hit)
                mCurrentTool.reset(new RotateRenderTreeNodeTool(*mState.entity, hitnode));
            else mCurrentTool.reset(new MoveRenderTreeNodeTool(*mState.entity, hitnode, snap, grid_size));

            mUI.tree->SelectItemById(app::FromUtf8(hitnode->GetId()));
        }
        else
        {
            mUI.tree->ClearSelection();
            mCurrentTool.reset(new MoveCameraTool(mState));
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

    if (mCurrentTool->MouseRelease(mickey, view))
    {
        mCurrentTool.release();
        UncheckPlacementActions();
    }
}

void EntityWidget::MouseDoubleClick(QMouseEvent* mickey)
{
    // double click is preceded by a regular click event and quick
    // googling suggests that there's really no way to filter out
    // single click when trying to react only to double click other
    // than to set a timer (which adds latency).
    // Going to simply discard any tool selection here on double click.
    mCurrentTool.release();

    gfx::Transform view;
    view.Scale(GetValue(mUI.scaleX), GetValue(mUI.scaleY));
    view.Scale(GetValue(mUI.zoom), GetValue(mUI.zoom));
    view.Rotate((float)qDegreesToRadians(mUI.rotation->value()));
    view.Translate(mState.camera_offset_x, mState.camera_offset_y);

    auto [hitnode, hitpos] = SelectNode(mickey->pos(), view, *mState.entity, GetCurrentNode());
    if (!hitnode  || !hitnode->GetDrawable())
        return;

    auto* drawable = hitnode->GetDrawable();
    QString material = app::FromUtf8(drawable->GetMaterialId());
    DlgMaterial dlg(this, mState.workspace, material);
    if (dlg.exec() == QDialog::Rejected)
        return;
    drawable->SetMaterialId(app::ToUtf8(dlg.GetSelectedMaterialId()));
}

bool EntityWidget::KeyPress(QKeyEvent* key)
{
    // handle key press events coming from the gfx widget
    if (mCurrentTool && mCurrentTool->KeyPress(key))
        return true;

    switch (key->key())
    {
        case Qt::Key_Delete:
            on_actionNodeDelete_triggered();
            break;
        case Qt::Key_W:
            TranslateCamera(0.0f, 20.0f);
            break;
        case Qt::Key_S:
            TranslateCamera(0.0f, -20.0f);
            break;
        case Qt::Key_A:
            TranslateCamera(20.0f, 0.0f);
            break;
        case Qt::Key_D:
            TranslateCamera(-20.0f, 0.0f);
            break;
        case Qt::Key_Left:
            TranslateCurrentNode(-20.0f, 0.0f);
            break;
        case Qt::Key_Right:
            TranslateCurrentNode(20.0f, 0.0f);
            break;
        case Qt::Key_Up:
            TranslateCurrentNode(0.0f, -20.0f);
            break;
        case Qt::Key_Down:
            TranslateCurrentNode(0.0f, 20.0f);
            break;
        case Qt::Key_Escape:
            if (mCurrentTool)
                mCurrentTool.reset();
            else mUI.tree->ClearSelection();
            break;
        default:
            return false;
    }
    return true;
}

void EntityWidget::DisplayCurrentNodeProperties()
{
    SetValue(mUI.nodeID, QString(""));
    SetValue(mUI.nodeName, QString(""));
    SetValue(mUI.nodeTranslateX, 0.0f);
    SetValue(mUI.nodeTranslateY, 0.0f);
    SetValue(mUI.nodeSizeX, 0.0f);
    SetValue(mUI.nodeSizeY, 0.0f);
    SetValue(mUI.nodeScaleX, 1.0f);
    SetValue(mUI.nodeScaleY, 1.0f);
    SetValue(mUI.nodeRotation, 0.0f);
    SetValue(mUI.drawableItem, false);
    SetValue(mUI.rigidBodyItem, false);
    SetValue(mUI.textItem, false);
    SetValue(mUI.dsMaterial, QString(""));
    SetValue(mUI.dsDrawable, QString(""));
    SetValue(mUI.dsLayer, 0);
    SetValue(mUI.dsRenderPass, game::DrawableItemClass::RenderPass::Draw);
    SetValue(mUI.dsRenderStyle, game::DrawableItemClass::RenderStyle::Solid);
    SetValue(mUI.dsLineWidth, 1.0f);
    SetValue(mUI.dsTimeScale, 1.0f);
    SetValue(mUI.dsAlpha, NormalizedFloat(1.0f));
    SetValue(mUI.rbFriction, 0.0f);
    SetValue(mUI.rbRestitution, 0.0f);
    SetValue(mUI.rbAngularDamping, 0.0f);
    SetValue(mUI.rbLinearDamping, 0.0f);
    SetValue(mUI.rbDensity, 0.0f);
    SetValue(mUI.rbLinearVeloX, 0.0f);
    SetValue(mUI.rbLinearVeloY, 0.0f);
    SetValue(mUI.rbAngularVelo, 0.0f);
    SetValue(mUI.rbIsBullet, false);
    SetValue(mUI.rbIsSensor, false);
    SetValue(mUI.rbIsEnabled, false);
    SetValue(mUI.rbCanSleep, false);
    SetValue(mUI.rbDiscardRotation, false);
    SetValue(mUI.tiFontName, QString(""));
    SetValue(mUI.tiFontSize, 16);
    SetValue(mUI.tiVAlign, game::TextItemClass::VerticalTextAlign::Center);
    SetValue(mUI.tiHAlign, game::TextItemClass::HorizontalTextAlign::Center);
    SetValue(mUI.tiTextColor, Qt::white);
    SetValue(mUI.tiLineHeight, 1.0f);
    SetValue(mUI.tiLayer, 0);
    SetValue(mUI.tiText, QString(""));
    SetValue(mUI.tiVisible, true);
    SetValue(mUI.tiUnderline, false);
    SetValue(mUI.tiBlink, false);
    SetEnabled(mUI.nodeProperties, false);
    SetEnabled(mUI.nodeTransform, false);
    SetEnabled(mUI.nodeItems, false);

    if (const auto* node = GetCurrentNode())
    {
        SetEnabled(mUI.nodeProperties, true);
        SetEnabled(mUI.nodeTransform, true);
        SetEnabled(mUI.nodeItems, true);

        const auto& translate = node->GetTranslation();
        const auto& size = node->GetSize();
        const auto& scale = node->GetScale();
        SetValue(mUI.nodeID, node->GetId());
        SetValue(mUI.nodeName, node->GetName());
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
            SetValue(mUI.dsTimeScale, item->GetTimeScale());
            SetValue(mUI.dsAlpha, NormalizedFloat(item->GetAlpha()));
            SetValue(mUI.dsVisible, item->TestFlag(game::DrawableItemClass::Flags::VisibleInGame));
            SetValue(mUI.dsUpdateDrawable, item->TestFlag(game::DrawableItemClass::Flags::UpdateDrawable));
            SetValue(mUI.dsUpdateMaterial, item->TestFlag(game::DrawableItemClass::Flags::UpdateMaterial));
            SetValue(mUI.dsRestartDrawable, item->TestFlag(game::DrawableItemClass::Flags::RestartDrawable));
            SetValue(mUI.dsOverrideAlpha, item->TestFlag(game::DrawableItemClass::Flags::OverrideAlpha));
            SetValue(mUI.dsFlipVertically, item->TestFlag(game::DrawableItemClass::Flags::FlipVertically));
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
            SetValue(mUI.rbLinearVeloX, body->GetLinearVelocity().x);
            SetValue(mUI.rbLinearVeloY, body->GetLinearVelocity().y);
            SetValue(mUI.rbAngularVelo, body->GetAngularVelocity());
            SetValue(mUI.rbIsBullet, body->TestFlag(game::RigidBodyItemClass::Flags::Bullet));
            SetValue(mUI.rbIsSensor, body->TestFlag(game::RigidBodyItemClass::Flags::Sensor));
            SetValue(mUI.rbIsEnabled, body->TestFlag(game::RigidBodyItemClass::Flags::Enabled));
            SetValue(mUI.rbCanSleep, body->TestFlag(game::RigidBodyItemClass::Flags::CanSleep));
            SetValue(mUI.rbDiscardRotation, body->TestFlag(game::RigidBodyItemClass::Flags::DiscardRotation));
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
        if (const auto* text = node->GetTextItem())
        {
            SetValue(mUI.textItem, true);
            SetValue(mUI.tiFontName, text->GetFontName());
            SetValue(mUI.tiFontSize, text->GetFontSize());
            SetValue(mUI.tiVAlign, text->GetVAlign());
            SetValue(mUI.tiHAlign, text->GetHAlign());
            SetValue(mUI.tiTextColor, text->GetTextColor());
            SetValue(mUI.tiLineHeight, text->GetLineHeight());
            SetValue(mUI.tiLayer, text->GetLayer());
            SetValue(mUI.tiText, text->GetText());
            SetValue(mUI.tiVisible, text->TestFlag(game::TextItemClass::Flags::VisibleInGame));
            SetValue(mUI.tiUnderline, text->TestFlag(game::TextItemClass::Flags::UnderlineText));
            SetValue(mUI.tiBlink, text->TestFlag(game::TextItemClass::Flags::BlinkText));
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
    mUI.actionNewSemiCircle->setChecked(false);
}

void EntityWidget::TranslateCamera(float dx, float dy)
{
    mState.camera_offset_x += dx;
    mState.camera_offset_y += dy;
    DisplayCurrentCameraLocation();
}

void EntityWidget::TranslateCurrentNode(float dx, float dy)
{
    if (auto* node = GetCurrentNode())
    {
        auto pos = node->GetTranslation();
        pos.x += dx;
        pos.y += dy;
        node->SetTranslation(pos);
        SetValue(mUI.nodeTranslateX, pos.x);
        SetValue(mUI.nodeTranslateY, pos.y);
    }
}

void EntityWidget::UpdateCurrentNodeProperties()
{
    auto* node = GetCurrentNode();
    if (node == nullptr)
        return;

    node->SetRotation(qDegreesToRadians((float)GetValue(mUI.nodeRotation)));

    if (auto* item = node->GetDrawable())
    {
        const float alpha_val = mUI.dsAlpha->value();
        const float alpha_max = mUI.dsAlpha->maximum();
        const float alpha = alpha_val / alpha_max;

        const QString& material_name = GetValue(mUI.dsMaterial);
        const QString& drawable_name = GetValue(mUI.dsDrawable);
        item->SetDrawableId(mState.workspace->GetDrawableClassByName(drawable_name)->GetId());
        item->SetMaterialId(mState.workspace->GetMaterialClassByName(material_name)->GetId());
        item->SetTimeScale(GetValue(mUI.dsTimeScale));
        item->SetLineWidth(GetValue(mUI.dsLineWidth));
        item->SetLayer(GetValue(mUI.dsLayer));
        item->SetRenderStyle(GetValue(mUI.dsRenderStyle));
        item->SetRenderPass(GetValue(mUI.dsRenderPass));
        item->SetAlpha(alpha);

        item->SetFlag(game::DrawableItemClass::Flags::VisibleInGame, GetValue(mUI.dsVisible));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateDrawable, GetValue(mUI.dsUpdateDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::UpdateMaterial, GetValue(mUI.dsUpdateMaterial));
        item->SetFlag(game::DrawableItemClass::Flags::RestartDrawable, GetValue(mUI.dsRestartDrawable));
        item->SetFlag(game::DrawableItemClass::Flags::OverrideAlpha, GetValue(mUI.dsOverrideAlpha));
        item->SetFlag(game::DrawableItemClass::Flags::FlipVertically, GetValue(mUI.dsFlipVertically));
    }

    if (auto* body = node->GetRigidBody())
    {
        glm::vec2 linear_velocity;
        linear_velocity.x = GetValue(mUI.rbLinearVeloX);
        linear_velocity.y = GetValue(mUI.rbLinearVeloY);
        const QString& shape_name  = GetValue(mUI.rbPolygon);
        if (!shape_name.isEmpty())
            body->SetPolygonShapeId(mState.workspace->GetDrawableClassByName(shape_name)->GetId());
        body->SetSimulation(GetValue(mUI.rbSimulation));
        body->SetCollisionShape(GetValue(mUI.rbShape));
        body->SetFriction(GetValue(mUI.rbFriction));
        body->SetRestitution(GetValue(mUI.rbRestitution));
        body->SetAngularDamping(GetValue(mUI.rbAngularDamping));
        body->SetLinearDamping(GetValue(mUI.rbLinearDamping));
        body->SetDensity(GetValue(mUI.rbDensity));
        body->SetAngularVelocity(GetValue(mUI.rbAngularVelo));
        body->SetLinearVelocity(linear_velocity);

        // flags
        body->SetFlag(game::RigidBodyItemClass::Flags::Bullet, GetValue(mUI.rbIsBullet));
        body->SetFlag(game::RigidBodyItemClass::Flags::Sensor, GetValue(mUI.rbIsSensor));
        body->SetFlag(game::RigidBodyItemClass::Flags::Enabled, GetValue(mUI.rbIsEnabled));
        body->SetFlag(game::RigidBodyItemClass::Flags::CanSleep, GetValue(mUI.rbCanSleep));
        body->SetFlag(game::RigidBodyItemClass::Flags::DiscardRotation, GetValue(mUI.rbDiscardRotation));
    }

    if (auto* text = node->GetTextItem())
    {
        text->SetFontName(GetValue(mUI.tiFontName));
        text->SetFontSize(GetValue(mUI.tiFontSize));
        text->SetAlign((game::TextItemClass::VerticalTextAlign)GetValue(mUI.tiVAlign));
        text->SetAlign((game::TextItemClass::HorizontalTextAlign)GetValue(mUI.tiHAlign));
        text->SetTextColor(GetValue(mUI.tiTextColor));
        text->SetLineHeight(GetValue(mUI.tiLineHeight));
        text->SetText(GetValue(mUI.tiText));
        text->SetLayer(GetValue(mUI.tiLayer));
        // flags
        text->SetFlag(game::TextItemClass::Flags::VisibleInGame, GetValue(mUI.tiVisible));
        text->SetFlag(game::TextItemClass::Flags::UnderlineText, GetValue(mUI.tiUnderline));
        text->SetFlag(game::TextItemClass::Flags::BlinkText, GetValue(mUI.tiBlink));
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
        else if (resource.GetType() == app::Resource::Type::Shape)
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

    std::vector<ListItem> polygons;
    std::vector<ListItem> scripts;
    // for the rigid body we need to list the polygonal (custom) shape
    // objects. (note that it's actually possible that these would be concave
    // but this case isn't currently supported)
    for (size_t i=0; i<mState.workspace->GetNumUserDefinedResources(); ++i)
    {
        const auto& res = mState.workspace->GetUserDefinedResource(i);
        ListItem pair;
        pair.name = res.GetName();
        pair.id   = res.GetId();
        if (res.GetType() == app::Resource::Type::Shape) {
            polygons.push_back(pair);
        } else if (res.GetType() == app::Resource::Type::Script) {
            scripts.push_back(pair);
        }
    }
    SetList(mUI.rbPolygon, polygons);
    SetList(mUI.scriptFile, scripts);
    SetValue(mUI.scriptFile, ListItemId(mState.entity->GetScriptFileId()));
}

void EntityWidget::UpdateDeletedResourceReferences()
{
    for (size_t i=0; i<mState.entity->GetNumNodes(); ++i)
    {
        auto& node = mState.entity->GetNode(i);
        if (auto* draw = node.GetDrawable())
        {
            const auto drawable = draw->GetDrawableId();
            const auto material = draw->GetMaterialId();
            if (!mState.workspace->IsValidMaterial(material))
            {
                WARN("Entity node '%1' uses material that is no longer available." , node.GetName());
                draw->ResetMaterial();
                draw->SetMaterialId("_checkerboard");
            }
            if (!mState.workspace->IsValidDrawable(drawable))
            {
                WARN("Entity node '%1' uses drawable that is no longer available." , node.GetName());
                draw->ResetDrawable();
                draw->SetDrawableId("_rect");
            }
        }
        if (auto* body = node.GetRigidBody())
        {
            if (body->GetCollisionShape() != game::RigidBodyItemClass::CollisionShape::Polygon)
                continue;
            const auto& polygon = body->GetPolygonShapeId();
            if (!mState.workspace->IsValidDrawable(polygon))
            {
                WARN("Entity node '%1' uses rigid body shape that is no longer available.", node.GetName());
                body->ResetPolygonShapeId();
            }
        }
    }

    if (mState.entity->HasScriptFile())
    {
        const auto& scriptId = mState.entity->GetScriptFileId();
        if (!mState.workspace->IsValidScript(scriptId))
        {
            WARN("Entity '%1' script is no longer available.", mState.entity->GetName());
            mState.entity->ResetScriptFile();
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

const game::EntityNodeClass* EntityWidget::GetCurrentNode() const
{
    TreeWidget::TreeItem* item = mUI.tree->GetSelectedItem();
    if (item == nullptr)
        return nullptr;
    else if (!item->GetUserData())
        return nullptr;
    return static_cast<game::EntityNodeClass*>(item->GetUserData());
}

} // bui

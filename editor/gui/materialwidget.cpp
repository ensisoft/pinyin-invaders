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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QPixmap>
#  include <QTextStream>
#  include <base64/base64.h>
#  include <nlohmann/json.hpp>
#  include <boost/algorithm/string/erase.hpp>
#include "warnpop.h"

#include <algorithm>

#include "base/assert.h"
#include "base/utility.h"
#include "base/json.h"
#include "data/json.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/transform.h"
#include "graphics/drawing.h"
#include "graphics/types.h"
#include "editor/app/eventlog.h"
#include "editor/app/resource.h"
#include "editor/app/utility.h"
#include "editor/app/workspace.h"
#include "editor/gui/settings.h"
#include "editor/gui/utility.h"
#include "editor/gui/drawing.h"
#include "editor/gui/dlgtext.h"
#include "editor/gui/dlgbitmap.h"
#include "editor/gui/uniform.h"
#include "editor/gui/sampler.h"
#include "editor/gui/dlgtexturerect.h"
#include "editor/gui/materialwidget.h"

namespace {

struct TexturePackImage {
    QString name;
    unsigned width  = 0;
    unsigned height = 0;
    unsigned xpos   = 0;
    unsigned ypos   = 0;
    unsigned index  = 0;
};

bool ReadTexturePack(const QString& json_file, std::vector<TexturePackImage>* out)
{
    QFile file(json_file);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open '%1' for reading. (%2)", json_file, file.error());
        return false;
    }
    const auto& buff = file.readAll();
    if (buff.isEmpty())
    {
        ERROR("JSON file '%1' contains no content.", json_file);
        return false;
    }
    const auto* beg  = buff.data();
    const auto* end  = buff.data() + buff.size();
    const auto& json = nlohmann::json::parse(beg, end, nullptr, false);
    if (json.is_discarded())
    {
        ERROR("JSON file '%1' could not be parsed.", json_file);
        return false;
    }
    if (!json.contains("images") || !json["images"].is_array())
    {
        ERROR("JSON file '%1' doesn't contain images array.");
        return false;
    }
    for (const auto& img_json : json["images"].items())
    {
        const auto& obj = img_json.value();
        std::string name;
        unsigned w, h, x, y, index;
        if (!base::JsonReadSafe(obj, "width", &w) ||
            !base::JsonReadSafe(obj, "height", &h) ||
            !base::JsonReadSafe(obj, "xpos", &x) ||
            !base::JsonReadSafe(obj, "ypos", &y) ||
            !base::JsonReadSafe(obj, "name", &name) ||
            !base::JsonReadSafe(obj, "index", &index))
        {
            ERROR("Failed to read JSON image box data.");
            continue;
        }
        TexturePackImage tpi;
        tpi.name   = app::FromUtf8(name);
        tpi.width  = w;
        tpi.height = h;
        tpi.xpos   = x;
        tpi.ypos   = y;
        tpi.index  = index;
        out->push_back(std::move(tpi));
    }

    // finally sort based on the image index.
    std::sort(std::begin(*out), std::end(*out), [&](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    INFO("Successfully parsed '%1'. %2 images found.", json_file, out->size());
    return true;
}

} // namespace

namespace gui
{

MaterialWidget::MaterialWidget(app::Workspace* workspace)
{
    DEBUG("Create MaterialWidget");
    mWorkspace = workspace;
    mMaterial  = std::make_shared<gfx::ColorClass>();

    mUI.setupUi(this);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onZoomIn = std::bind(&MaterialWidget::ZoomIn, this);
    mUI.widget->onZoomOut = std::bind(&MaterialWidget::ZoomOut, this);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);

    QMenu* menu  = new QMenu(this);
    QAction* add_texture_from_file = menu->addAction("File");
    QAction* add_texture_from_text = menu->addAction("Text");
    QAction* add_texture_from_bitmap = menu->addAction(("Bitmap"));
    connect(add_texture_from_file, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromFile);
    connect(add_texture_from_text, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromText);
    connect(add_texture_from_bitmap, &QAction::triggered, this, &MaterialWidget::AddNewTextureMapFromBitmap);
    mUI.btnAddTextureMap->setMenu(menu);

    PopulateFromEnum<gfx::MaterialClass::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::MaterialClass::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::MaterialClass::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::MaterialClass::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::MaterialClass::Type>(mUI.materialType);
    PopulateFromEnum<gfx::BuiltInMaterialClass::ParticleAction>(mUI.particleAction);

    SetList(mUI.cmbModel, workspace->ListPrimitiveDrawables());
    SetValue(mUI.cmbModel, "Rectangle");
    SetValue(mUI.materialID, mMaterial->GetId());
    SetValue(mUI.materialName, QString("My Material"));
    setWindowTitle("My Material");

    GetMaterialProperties();
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());
    mMaterial = resource.GetContent<gfx::MaterialClass>()->Copy();
    mOriginalHash = mMaterial->GetHash();
    SetValue(mUI.materialID, resource.GetId());
    SetValue(mUI.materialName, resource.GetName());
    GetUserProperty(resource, "model", mUI.cmbModel);
    GetUserProperty(resource, "zoom", mUI.zoom);
    GetUserProperty(resource, "widget", mUI.widget);

    ApplyShaderDescription();
    GetMaterialProperties();

    setWindowTitle(resource.GetName());
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");
}

void MaterialWidget::AddActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}
void MaterialWidget::AddActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
}

bool MaterialWidget::LoadState(const Settings& settings)
{
    std::string base64;
    settings.getValue("Material", "content", &base64);

    data::JsonObject json;
    auto [ok, error] = json.ParseString(base64::Decode(base64));
    if (!ok)
    {
        ERROR("Failed to parse content JSON. '%1'", error);
        return false;
    }
    auto ret = gfx::MaterialClass::FromJson(json);
    if (!ret)
    {
        WARN("Failed to load material widget state.");
        return false;
    }
    mMaterial = std::move(ret);
    mOriginalHash = mMaterial->GetHash();

    settings.loadWidget("Material", mUI.materialName);
    settings.loadWidget("Material", mUI.zoom);
    settings.loadWidget("Material", mUI.cmbModel);
    settings.loadWidget("Material", mUI.widget);

    ApplyShaderDescription();
    GetMaterialProperties();

    setWindowTitle(mUI.materialName->text());
    return true;
}

bool MaterialWidget::SaveState(Settings& settings) const
{
    data::JsonObject json;
    mMaterial->IntoJson(json);

    settings.saveWidget("Material", mUI.materialName);
    settings.saveWidget("Material", mUI.zoom);
    settings.saveWidget("Material", mUI.cmbModel);
    settings.saveWidget("Material", mUI.widget);
    settings.setValue("Material", "content", base64::Encode(json.ToString()));
    return true;
}

bool MaterialWidget::CanTakeAction(Actions action, const Clipboard* clipboard) const
{
    switch (action)
    {
        case Actions::CanCut:
        case Actions::CanCopy:
        case Actions::CanPaste:
            return false;
        case Actions::CanUndo:
            return false;
        case Actions::CanReloadTextures:
        case Actions::CanReloadShaders:
            return true;
        case Actions::CanZoomIn: {
            const auto max = mUI.zoom->maximum();
            const auto val = mUI.zoom->value();
            return val < max;
        }
        break;
        case Actions::CanZoomOut: {
            const auto min = mUI.zoom->minimum();
            const auto val = mUI.zoom->value();
            return val > min;
        }
        break;
    }
    BUG("Unhandled action query.");
    return false;
}

void MaterialWidget::ZoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void MaterialWidget::ZoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void MaterialWidget::ReloadShaders()
{
    mUI.widget->reloadShaders();
}
void MaterialWidget::ReloadTextures()
{
    mUI.widget->reloadTextures();
}

void MaterialWidget::Shutdown()
{
    mUI.widget->dispose();
}

void MaterialWidget::Update(double secs)
{
    if (mState == PlayState::Playing)
    {
        mTime += secs;
    }
}

void MaterialWidget::Save()
{
    on_actionSave_triggered();
}

bool MaterialWidget::HasUnsavedChanges() const
{
    if (!mOriginalHash)
        return false;
    const auto hash = mMaterial->GetHash();
    return hash != mOriginalHash;
}

bool MaterialWidget::ConfirmClose()
{
    // any unsaved changes ?
    const auto hash = mMaterial->GetHash();
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
bool MaterialWidget::GetStats(Stats* stats) const
{
    stats->time  = mTime;
    stats->fps   = mUI.widget->getCurrentFPS();
    stats->vsync = mUI.widget->haveVSYNC();
    return true;
}

void MaterialWidget::Render()
{
    mUI.widget->triggerPaint();
}

void MaterialWidget::on_actionPlay_triggered()
{
    mState = PlayState::Playing;
    mUI.actionPlay->setEnabled(false);
    mUI.actionPause->setEnabled(true);
    mUI.actionStop->setEnabled(true);
}

void MaterialWidget::on_actionPause_triggered()
{
    mState = PlayState::Paused;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(true);
}

void MaterialWidget::on_actionStop_triggered()
{
    mState = PlayState::Stopped;
    mUI.actionPlay->setEnabled(true);
    mUI.actionPause->setEnabled(false);
    mUI.actionStop->setEnabled(false);
    mTime = 0.0f;
}

void MaterialWidget::on_actionSave_triggered()
{
    if (!MustHaveInput(mUI.materialName))
        return;

    app::MaterialResource resource(mMaterial, GetValue(mUI.materialName));
    SetUserProperty(resource, "model", mUI.cmbModel);
    SetUserProperty(resource, "widget", mUI.widget);    
    SetUserProperty(resource, "zoom", mUI.zoom);
    mWorkspace->SaveResource(resource);
    mOriginalHash = mMaterial->GetHash();

    NOTE("Saved material '%1'", GetValue(mUI.materialName));
    INFO("Saved material '%1'", GetValue(mUI.materialName));
    setWindowTitle(GetValue(mUI.materialName));
}
void MaterialWidget::on_actionRemoveTexture_triggered()
{
    // make sure currentRowChanged is blocked while we're deleting shit.
    QSignalBlocker block(mUI.textures);
    QList<QListWidgetItem*> items = mUI.textures->selectedItems();

    if (auto* texture = mMaterial->AsTexture())
    {
        texture->ResetTexture();
    }
    else if (auto* sprite = mMaterial->AsSprite())
    {
        for (int i=0; i<items.size(); ++i)
        {
            const QListWidgetItem* item = items[i];
            const auto& data = item->data(Qt::UserRole).toString();
            sprite->DeleteTextureById(app::ToUtf8(data));
        }
    }
    else if (auto* custom = mMaterial->AsCustom())
    {
        for (int i=0; i<items.size(); ++i)
        {
            const QListWidgetItem* item = items[i];
            const auto& data = item->data(Qt::UserRole).toString();
            const auto* source = custom->FindTextureSourceById(app::ToUtf8(data));
            custom->DeleteTextureSource(source);
        }
    }

    qDeleteAll(items);

    GetMaterialProperties();
}

void MaterialWidget::on_btnReloadShader_clicked()
{
    ApplyShaderDescription();
    GetMaterialProperties();
    ReloadShaders();
}

void MaterialWidget::on_btnSelectShader_clicked()
{
    if (auto* custom = mMaterial->AsCustom())
    {
        const auto& ret = QFileDialog::getOpenFileName(this,
            tr("Select Shader File"), "",
            tr("Shaders (*.glsl)"));
        if (ret.isEmpty()) return;
        const auto& uri = mWorkspace->MapFileToWorkspace(ret);
        custom->SetShaderUri(app::ToUtf8(uri));
        ApplyShaderDescription();
        ReloadShaders();
        GetMaterialProperties();
    }
}

void MaterialWidget::on_btnAddTextureMap_clicked()
{
    mUI.btnAddTextureMap->showMenu();
}

void MaterialWidget::on_btnDelTextureMap_clicked()
{
    if (auto* texture = mMaterial->AsTexture())
        texture->ResetTexture();
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->ResetTextures();
    else if (auto* custom = mMaterial->AsCustom())
    {
        auto* widget = qobject_cast<Sampler*>(sender());
        auto* map = custom->FindTextureMap(app::ToUtf8(widget->GetName()));
        map->ResetTextures();
    }

    GetMaterialProperties();
}

void MaterialWidget::on_btnEditTexture_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::TextureSource* source = nullptr;

    if (auto* texture = mMaterial->AsTexture())
        source = texture->GetTextureSource();
    else if (auto* sprite = mMaterial->AsSprite())
        source = sprite->GetTextureSource(row);
    else if (auto* custom = mMaterial->AsCustom())
        source = custom->FindTextureSourceById(GetItemId(mUI.textures));
    if (source == nullptr) return;

    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(source))
    {
        emit OpenExternalImage(app::FromUtf8(ptr->GetFilename()));
    }
    else if (auto* ptr = dynamic_cast<gfx::detail::TextureTextBufferSource*>(source))
    {
        // make a copy for editing.
        gfx::TextBuffer text(ptr->GetTextBuffer());
        DlgText dlg(this, text);
        if (dlg.exec() == QDialog::Rejected)
            return;

        // map the font files.
        for (size_t i=0; i<text.GetNumTexts(); ++i)
        {
            auto& style_and_text = text.GetText(i);
            style_and_text.font  = mWorkspace->MapFileToWorkspace(style_and_text.font);
        }

        // Update the texture source's TextBuffer
        ptr->SetTextBuffer(std::move(text));

        // update the preview.
        on_textures_currentRowChanged(row);
    }
    else if (auto* ptr = dynamic_cast<gfx::detail::TextureBitmapGeneratorSource*>(source))
    {
        // make a copy for editing.
        auto copy = ptr->GetGenerator().Clone();
        DlgBitmap dlg(this, std::move(copy));
        if (dlg.exec() == QDialog::Rejected)
            return;

        // override the generator with changes.
        ptr->SetGenerator(dlg.GetResult());

        // update the preview.
        on_textures_currentRowChanged(row);
    }
}

void MaterialWidget::on_btnResetTextureRect_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    if (auto* texture = mMaterial->AsTexture())
        texture->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->SetTextureRect(row, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    else if (auto* custom = mMaterial->AsCustom())
    {
        const auto* source = custom->FindTextureSourceById(GetItemId(mUI.textures));
        custom->SetTextureSourceRect(source, gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }

    GetTextureProperties();
}

void MaterialWidget::on_btnSelectTextureRect_clicked()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;
    gfx::TextureSource* source = nullptr;
    gfx::FRect rect;
    if (auto* texture = mMaterial->AsTexture())
    {
        source = texture->GetTextureSource();
        rect   = texture->GetTextureRect();
    }
    else if (auto* sprite = mMaterial->AsSprite())
    {
        source = sprite->GetTextureSource(row);
        rect   = sprite->GetTextureRect(row);
    }
    else if (auto* custom = mMaterial->AsCustom())
    {
        source = custom->FindTextureSourceById(GetItemId(mUI.textures));
        rect   = custom->FindTextureSourceRect(source);
    }

    DlgTextureRect dlg(this, rect, source->Clone());
    if (dlg.exec() == QDialog::Rejected)
        return;

    if (auto* texture = mMaterial->AsTexture())
        texture->SetTextureRect(dlg.GetRect());
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->SetTextureRect(row, dlg.GetRect());
    else if (auto* custom = mMaterial->AsCustom())
        custom->SetTextureSourceRect(source, dlg.GetRect());

    GetTextureProperties();
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{ GetTextureProperties(); }
void MaterialWidget::on_textures_customContextMenuRequested(const QPoint&)
{
    const auto& items = mUI.textures->selectedItems();
    SetEnabled(mUI.actionRemoveTexture, !items.isEmpty());

    QMenu menu(this);
    menu.addAction(mUI.actionRemoveTexture);
    menu.exec(QCursor::pos());
}

void MaterialWidget::on_materialType_currentIndexChanged(int)
{
    // important: make sure not to change the material class id
    // even though we're creating a new material class object.
    const auto klassid = mMaterial->GetId();

    const gfx::MaterialClass::Type type = GetValue(mUI.materialType);
    if (type == mMaterial->GetType()) return;
    else if (type == gfx::MaterialClass::Type::Color)
        mMaterial.reset(new gfx::ColorClass);
    else if (type == gfx::MaterialClass::Type::Gradient)
        mMaterial.reset(new gfx::GradientClass);
    else if (type == gfx::MaterialClass::Type::Sprite)
        mMaterial.reset(new gfx::SpriteClass);
    else if (type == gfx::MaterialClass::Type::Texture)
        mMaterial.reset(new gfx::TextureMap2DClass);
    else if (type == gfx::MaterialClass::Type::Custom)
        mMaterial.reset( new gfx::CustomMaterialClass);
    else BUG("Unhandled material type.");

    // see the comment above about the class id!
    mMaterial->SetId(klassid);

    ApplyShaderDescription();

    GetMaterialProperties();
}
void MaterialWidget::on_surfaceType_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_gamma_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_baseColor_colorChanged(QColor color)
{ SetMaterialProperties(); }
void MaterialWidget::on_particleAction_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_spriteFps_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkStaticInstance_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_chkBlendFrames_stateChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap0_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap1_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap2_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_colorMap3_colorChanged(QColor)
{ SetMaterialProperties(); }
void MaterialWidget::on_gradientOffsetX_valueChanged(int value)
{ SetMaterialProperties(); }
void MaterialWidget::on_gradientOffsetY_valueChanged(int value)
{ SetMaterialProperties(); }
void MaterialWidget::on_scaleX_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_scaleY_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityX_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityY_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_velocityZ_valueChanged(double)
{ SetMaterialProperties(); }
void MaterialWidget::on_minFilter_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_magFilter_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_wrapX_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_wrapY_currentIndexChanged(int)
{ SetMaterialProperties(); }
void MaterialWidget::on_rectX_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectY_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectW_valueChanged(double value)
{ SetTextureRect(); }
void MaterialWidget::on_rectH_valueChanged(double value)
{ SetTextureRect(); }

void MaterialWidget::AddNewTextureMapFromFile()
{
    if (auto* texture = mMaterial->AsTexture())
    {
        const auto ret = QFileDialog::getOpenFileName(this,
            tr("Select Image File"), "",
            tr("Images (*.png *.jpg *.jpeg)"));
        if (ret.isEmpty()) return;

        const QFileInfo info(ret);
        const auto& name = info.baseName();
        const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());
        auto source = std::make_unique<gfx::detail::TextureFileSource>(app::ToUtf8(file));

        source->SetName(app::ToUtf8(name));
        texture->SetTexture(std::move(source));
        texture->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
    }
    else if (auto* sprite = mMaterial->AsSprite())
    {
        const auto& list = QFileDialog::getOpenFileNames(this,
            tr("Select Image File(s)"), "",
            tr("Images (*.png *.jpg *.jpeg)"));
        if (list.isEmpty()) return;

        for (const auto& item : list)
        {
            const QFileInfo info(item);
            const auto& name = info.baseName();
            const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());
            auto source = std::make_unique<gfx::detail::TextureFileSource>(app::ToUtf8(file));

            source->SetName(app::ToUtf8(name));
            sprite->AddTexture(std::move(source));
            sprite->SetTextureRect(0,gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        }
    }
    else if (auto* custom = mMaterial->AsCustom())
    {
        auto* widget = qobject_cast<Sampler*>(sender());
        auto* map = custom->FindTextureMap(app::ToUtf8(widget->GetName()));
        if (auto* texture = map->AsTextureMap2D())
        {
            const auto ret = QFileDialog::getOpenFileName(this,
                tr("Select Image File"), "",
                tr("Images (*.png *.jpg *.jpeg)"));
            if (ret.isEmpty()) return;

            const QFileInfo info(ret);
            const auto& name = info.baseName();
            const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());
            auto source = std::make_unique<gfx::detail::TextureFileSource>(app::ToUtf8(file));

            source->SetName(app::ToUtf8(name));
            texture->SetTexture(std::move(source));
            texture->SetTextureRect(gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
        }
        else if (auto* sprite = map->AsSpriteMap())
        {
            const auto& list = QFileDialog::getOpenFileNames(this,
                tr("Select Image File(s)"), "",
                tr("Images (*.png *.jpg *.jpeg)"));
            if (list.isEmpty()) return;

            for (const auto& item : list)
            {
                const QFileInfo info(item);
                const auto& name = info.baseName();
                const auto& file = mWorkspace->MapFileToWorkspace(info.absoluteFilePath());
                auto source = std::make_unique<gfx::detail::TextureFileSource>(app::ToUtf8(file));

                source->SetName(app::ToUtf8(name));
                sprite->AddTexture(std::move(source));
                sprite->SetTextureRect(size_t(0), gfx::FRect(0.0f, 0.0f, 1.0f, 1.0f));
            }
        }
    }

    GetMaterialProperties();
}

void MaterialWidget::AddNewTextureMapFromText()
{
    // anything set in this text buffer will be default
    // when the dialog is opened.
    gfx::TextBuffer text(100, 100);
    DlgText dlg(this, text);
    if (dlg.exec() == QDialog::Rejected)
        return;

    // map the selected font files to the workspace.
    for (size_t i = 0; i < text.GetNumTexts(); ++i)
    {
        auto& style_and_text = text.GetText(i);
        style_and_text.font = mWorkspace->MapFileToWorkspace(style_and_text.font);
    }

    auto source = std::make_unique<gfx::detail::TextureTextBufferSource>(std::move(text));
    source->SetName("TextBuffer");

    if (auto* texture = mMaterial->AsTexture())
        texture->SetTexture(std::move(source));
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->AddTexture(std::move(source));
    else if (auto* custom = mMaterial->AsCustom())
    {
        auto* widget = qobject_cast<Sampler*>(sender());
        auto* map = custom->FindTextureMap(app::ToUtf8(widget->GetName()));
        if (auto* texture = map->AsTextureMap2D())
            texture->SetTexture(std::move(source));
        else if (auto* sprite = map->AsSpriteMap())
            sprite->AddTexture(std::move(source));
    }

    GetMaterialProperties();
}
void MaterialWidget::AddNewTextureMapFromBitmap()
{
    auto generator = std::make_unique<gfx::NoiseBitmapGenerator>();
    generator->SetWidth(100);
    generator->SetHeight(100);
    DlgBitmap dlg(this, std::move(generator));
    if (dlg.exec() == QDialog::Rejected)
        return;

    auto result = dlg.GetResult();
    auto source = std::make_unique<gfx::detail::TextureBitmapGeneratorSource>(std::move(result));
    source->SetName("Noise");

    if (auto* texture = mMaterial->AsTexture())
        texture->SetTexture(std::move(source));
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->AddTexture(std::move(source));
    else if (auto* custom = mMaterial->AsCustom())
    {
        auto* widget = qobject_cast<Sampler*>(sender());
        auto* map = custom->FindTextureMap(app::ToUtf8(widget->GetName()));
        if (auto* texture = map->AsTextureMap2D())
            texture->SetTexture(std::move(source));
        else if (auto* sprite = map->AsSpriteMap())
            sprite->AddTexture(std::move(source));
    }

    GetMaterialProperties();
}
void MaterialWidget::UniformValueChanged(const Uniform* uniform)
{
    //DEBUG("Uniform '%1' was modified.", uniform->GetName());
    SetMaterialProperties();
}

void MaterialWidget::ApplyShaderDescription()
{
    // sometimes Qt just f*n hates you. There's no simple/easy way to
    // just recreate a layout with a bunch of widgets! You cannot set a
    // layout when a layout already exists on a widget but deleting it
    // doesn't work as expected either!!
    // https://stackoverflow.com/questions/4272196/qt-remove-all-widgets-from-layout
    if (auto* layout = mUI.customUniforms->layout())
    {
        while (auto* item = layout->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
    }
    if (auto* layout = mUI.customSamplers->layout())
    {
        while (auto* item = layout->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
    }
    mUniforms.clear();
    mSamplers.clear();

    auto* material = mMaterial->AsCustom();
    if (!material) return;

    // try to load the .json file that should contain the meta information
    // about the shader input parameters.
    auto uri = material->GetShaderUri();
    if (uri.empty()) return;
    boost::replace_all(uri, ".glsl", ".json");
    const auto [ok, json, error] = base::JsonParseFile(app::ToUtf8(mWorkspace->MapFileToFilesystem(uri)));
    if (!ok) ERROR_RETURN("Failed to load the shader description file '%1' %2", uri, error);

    if (json.contains("uniforms"))
    {
        if (!mUI.customUniforms->layout())
            mUI.customUniforms->setLayout(new QGridLayout);
        auto* layout = qobject_cast<QGridLayout*>(mUI.customUniforms->layout());

        auto uniforms = material->GetUniforms();
        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json["uniforms"].items())
        {
            Uniform::Type type;
            std::string name;
            std::string desc;
            if (!base::JsonReadSafe(json.value(), "desc", &desc) ||
                !base::JsonReadSafe(json.value(), "name", &name) ||
                !base::JsonReadSafe(json.value(), "type", &type))
            {
                WARN("Failed to understand shader uniform description.");
                continue;
            }
            auto* label = new QLabel(this);
            SetValue(label, desc);
            layout->addWidget(label, widget_row, 0);

            auto* widget = new Uniform(this);
            connect(widget, &Uniform::ValueChanged, this, &MaterialWidget::UniformValueChanged);
            widget->SetType(type);
            widget->SetName(app::FromUtf8(name));
            layout->addWidget(widget, widget_row, 1);
            mUniforms.push_back(widget);

            widget_row++;
            DEBUG("Read uniform description '%1'", name);
            uniforms.erase(name);

            // if the uniform already exists *and* has the matching type then
            // don't reset anything.
            if (type == Uniform::Type::Float && material->HasUniform<float>(name) ||
                type == Uniform::Type::Vec2  && material->HasUniform<glm::vec2>(name) ||
                type == Uniform::Type::Vec3  && material->HasUniform<glm::vec3>(name) ||
                type == Uniform::Type::Vec4  && material->HasUniform<glm::vec4>(name) ||
                type == Uniform::Type::Color && material->HasUniform<gfx::Color4f>(name))
                continue;

            // set default uniform value if it doesn't exist already.
            if (type == Uniform::Type::Float)
                material->SetUniform(name, 0.0f);
            else if (type == Uniform::Type::Vec2)
                material->SetUniform(name, glm::vec2());
            else if (type == Uniform::Type::Vec3)
                material->SetUniform(name, glm::vec3());
            else if (type == Uniform::Type::Vec4)
                material->SetUniform(name, glm::vec4());
            else if (type == Uniform::Type::Color)
                material->SetUniform(name, gfx::Color::White);
            else BUG("Unhandled uniform type.");
        }
        // delete the material uniforms that were no longer in the description
        for (auto pair : uniforms)
        {
            material->DeleteUniform(pair.first);
        }
    }
    else
    {
        material->DeleteUniforms();
    }

    if (json.contains("maps"))
    {
        if (!mUI.customSamplers->layout())
            mUI.customSamplers->setLayout(new QGridLayout);
        auto* layout = qobject_cast<QGridLayout*>(mUI.customSamplers->layout());

        auto maps = material->GetTextureMapNames();

        auto widget_row = 0;
        auto widget_col = 0;
        for (const auto& json : json["maps"].items())
        {
            std::string desc;
            std::string name;
            gfx::TextureMap::Type type;
            if (!base::JsonReadSafe(json.value(), "desc", &desc) ||
                !base::JsonReadSafe(json.value(), "name", &name) ||
                !base::JsonReadSafe(json.value(), "type", &type))
            {
                WARN("Failed to understand shader texture map description.");
                continue;
            }
            auto* label = new QLabel(this);
            SetValue(label, desc);
            layout->addWidget(label, widget_row, 0);

            auto* widget = new Sampler(this);
            connect(widget, &Sampler::AddNewTextureMapFromText, this, &MaterialWidget::AddNewTextureMapFromText);
            connect(widget, &Sampler::AddNewTextureMapFromFile, this, &MaterialWidget::AddNewTextureMapFromFile);
            connect(widget, &Sampler::AddNewTextureMapFromBitmap, this, &MaterialWidget::AddNewTextureMapFromBitmap);
            connect(widget, &Sampler::DelTextureMap, this, &MaterialWidget::on_btnDelTextureMap_clicked);
            connect(widget, &Sampler::SpriteFpsValueChanged, this, &MaterialWidget::on_spriteFps_valueChanged);
            widget->SetName(app::FromUtf8(name));
            if (type == gfx::TextureMap::Type::Texture2D)
                widget->ShowFps(false);
            else if (type == gfx::TextureMap::Type::Sprite)
                widget->ShowFps(true);
            else BUG("???");
            layout->addWidget(widget, widget_row, 1);
            mSamplers.push_back(widget);

            widget_row++;
            DEBUG("Read texture map description '%1'", name);
            maps.erase(name);

            // if the texture map already exists and has the matching type
            // then no need to do anything.
            if (material->HasTextureMap(name) &&
                material->FindTextureMap(name)->GetType() == type)
                continue;

            // create new texture map and add to the material
            if (type == gfx::TextureMap::Type::Texture2D)
            {
                std::string sampler_name;
                std::string texture_rect_uniform_name;
                if (!base::JsonReadSafe(json.value(), "sampler", &sampler_name))
                    WARN("Texture map '%1' has no name for texture sampler.", name);
                if (!base::JsonReadSafe(json.value(), "rect", &texture_rect_uniform_name))
                    WARN("Texture map '%1' has no name for texture rectangle uniform.", name);

                auto map = std::make_unique<gfx::TextureMap2D>();
                map->SetRectUniformName(std::move(texture_rect_uniform_name));
                map->SetSamplerName(std::move(sampler_name));
                material->SetTextureMap(name, std::move(map));
            }
            else if (type == gfx::TextureMap::Type::Sprite)
            {
                std::string sampler_name0;
                std::string sampler_name1;
                std::string texture_rect_uniform_name0;
                std::string texture_rect_uniform_name1;
                if (!base::JsonReadSafe(json.value(), "sampler0", &sampler_name0))
                    WARN("Texture map '%1' has no name for texture sampler 0.", name);
                if (!base::JsonReadSafe(json.value(), "sampler1", &sampler_name1))
                    WARN("Texture map '%1' has no name for texture sampler 1.", name);
                if (!base::JsonReadSafe(json.value(), "rect0", &texture_rect_uniform_name0))
                    WARN("Texture map '%1' has no name for texture 0 rectangle uniform.", name);
                if (!base::JsonReadSafe(json.value(), "rect1", &texture_rect_uniform_name1))
                    WARN("Texture map '%1' has no name for texture 0 rectangle uniform.", name);

                auto map = std::make_unique<gfx::SpriteMap>();
                map->SetSamplerName(sampler_name0, 0);
                map->SetSamplerName(sampler_name1, 1);
                map->SetRectUniformName(texture_rect_uniform_name0, 0);
                map->SetRectUniformName(texture_rect_uniform_name1, 1);
                material->SetTextureMap(name, std::move(map));
            }
            else BUG("Unhandled texture map type.");
        }
        // delete texture maps that were no longer in the description from the material
        for (auto name : maps)
        {
            material->DeleteTextureMap(name);
        }
    }
    else
    {
        material->DeleteTextureMaps();
    }
    INFO("Loaded shader description '%1'", uri);
}

void MaterialWidget::SetTextureRect()
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect(GetValue(mUI.rectX),
                    GetValue(mUI.rectY),
                    GetValue(mUI.rectW),
                    GetValue(mUI.rectH));
    if (auto* texture = mMaterial->AsTexture())
        texture->SetTextureRect(rect);
    else if (auto* sprite = mMaterial->AsSprite())
        sprite->SetTextureRect(row, rect);
    else if (auto* custom = mMaterial->AsCustom())
    {
        auto* source = custom->FindTextureSourceById(GetItemId(mUI.textures));
        ASSERT(source);
        custom->SetTextureSourceRect(source, rect);
    }
}

void MaterialWidget::SetMaterialProperties()
{
    if (auto* ptr = mMaterial->AsBuiltIn())
    {
        ptr->SetStatic(GetValue(mUI.chkStaticInstance));
        ptr->SetSurfaceType(GetValue(mUI.surfaceType));
        ptr->SetGamma(GetValue(mUI.gamma));
    }

    if (auto* ptr = mMaterial->AsCustom())
    {
        ptr->SetSurfaceType(GetValue(mUI.surfaceType));
        ptr->SetTextureMinFilter(GetValue(mUI.minFilter));
        ptr->SetTextureMagFilter(GetValue(mUI.magFilter));
        ptr->SetTextureWrapX(GetValue(mUI.wrapX));
        ptr->SetTextureWrapY(GetValue(mUI.wrapY));
        for (auto* widget : mUniforms)
        {
            const auto& name = app::ToUtf8(widget->GetName());
            if (auto* val = ptr->GetUniformValue<float>(name))
                *val = widget->GetAsFloat();
            else if (auto* val = ptr->GetUniformValue<glm::vec2>(name))
                *val = widget->GetAsVec2();
            else if (auto* val = ptr->GetUniformValue<glm::vec3>(name))
                *val = widget->GetAsVec3();
            else if (auto* val = ptr->GetUniformValue<glm::vec4>(name))
                *val = widget->GetAsVec4();
            else if (auto* val = ptr->GetUniformValue<gfx::Color4f>(name))
                *val = ToGfx(widget->GetAsColor());
            else BUG("No such uniform in material. UI and material are out of sync.");
        }
        for (auto* widget : mSamplers)
        {
            const auto& name = app::ToUtf8(widget->GetName());
            if (auto* map = ptr->FindTextureMap(name))
            {
                if (auto* sprite = map->AsSpriteMap())
                    sprite->SetFps(widget->GetSpriteFps());
            }
        }
    }
    else if (auto* ptr = mMaterial->AsColor())
    {
        ptr->SetBaseColor(GetValue(mUI.baseColor));
    }
    else if (auto* ptr = mMaterial->AsGradient())
    {
        ptr->SetColor(GetValue(mUI.colorMap0), gfx::GradientClass::ColorIndex::TopLeft);
        ptr->SetColor(GetValue(mUI.colorMap1), gfx::GradientClass::ColorIndex::TopRight);
        ptr->SetColor(GetValue(mUI.colorMap2), gfx::GradientClass::ColorIndex::BottomLeft);
        ptr->SetColor(GetValue(mUI.colorMap3), gfx::GradientClass::ColorIndex::BottomRight);
        glm::vec2 offset;
        offset.x = GetNormalizedValue(mUI.gradientOffsetX);
        offset.y = GetNormalizedValue(mUI.gradientOffsetY);
        ptr->SetOffset(offset);
    }
    else if (auto* ptr = mMaterial->AsTexture())
    {
        ptr->SetParticleAction(GetValue(mUI.particleAction));
        ptr->SetBaseColor(GetValue(mUI.baseColor));
        ptr->SetTextureMinFilter(GetValue(mUI.minFilter));
        ptr->SetTextureMagFilter(GetValue(mUI.magFilter));
        ptr->SetTextureScaleX(GetValue(mUI.scaleX));
        ptr->SetTextureScaleY(GetValue(mUI.scaleY));
        ptr->SetTextureWrapX(GetValue(mUI.wrapX));
        ptr->SetTextureWrapY(GetValue(mUI.wrapY));
        ptr->SetTextureVelocityX(GetValue(mUI.velocityX));
        ptr->SetTextureVelocityY(GetValue(mUI.velocityY));
        ptr->SetTextureVelocityZ(GetValue(mUI.velocityZ));
    }
    else if (auto* ptr = mMaterial->AsSprite())
    {
        ptr->SetBaseColor(GetValue(mUI.baseColor));
        ptr->SetParticleAction(GetValue(mUI.particleAction));
        ptr->SetTextureMinFilter(GetValue(mUI.minFilter));
        ptr->SetTextureMagFilter(GetValue(mUI.magFilter));
        ptr->SetTextureScaleX(GetValue(mUI.scaleX));
        ptr->SetTextureScaleY(GetValue(mUI.scaleY));
        ptr->SetTextureWrapX(GetValue(mUI.wrapX));
        ptr->SetTextureWrapY(GetValue(mUI.wrapY));
        ptr->SetTextureVelocityX(GetValue(mUI.velocityX));
        ptr->SetTextureVelocityY(GetValue(mUI.velocityY));
        ptr->SetTextureVelocityZ(GetValue(mUI.velocityZ));
        ptr->SetFps(GetValue(mUI.spriteFps));
        ptr->SetBlendFrames(GetValue(mUI.chkBlendFrames));
    }
}

void MaterialWidget::GetMaterialProperties()
{
    SetEnabled(mUI.shaderFile,        false);
    SetEnabled(mUI.gamma,             false);
    SetEnabled(mUI.baseColor,         false);
    SetEnabled(mUI.particleAction,    false);
    SetEnabled(mUI.spriteFps,         false);
    SetEnabled(mUI.btnAddTextureMap,  false);
    SetEnabled(mUI.btnDelTextureMap,  false);
    SetEnabled(mUI.chkStaticInstance, false);
    SetEnabled(mUI.chkBlendFrames,    false);
    SetEnabled(mUI.gradientMap,       false);
    SetEnabled(mUI.textureCoords,     false);
    SetEnabled(mUI.textureFilters,    false);
    SetEnabled(mUI.textureMaps,       false);
    SetEnabled(mUI.btnReloadShader,   false);
    SetEnabled(mUI.btnSelectShader,   false);
    SetVisible(mUI.builtInProperties, false);
    SetVisible(mUI.gradientMap,       false);
    SetVisible(mUI.textureCoords,     false);
    SetVisible(mUI.textureFilters,    false);
    SetVisible(mUI.customUniforms,    false);
    SetVisible(mUI.customSamplers,    false);

    // hide built-in properties and then show only the ones that apply
    SetVisible(mUI.baseColor,         false);
    SetVisible(mUI.lblBaseColor,      false);
    SetVisible(mUI.particleAction,    false);
    SetVisible(mUI.lblParticleEffect, false);
    SetVisible(mUI.spriteFps,         false);
    SetVisible(mUI.lblSpriteFps,      false);
    SetVisible(mUI.textureName,       false);
    SetVisible(mUI.lblTexture,        false);
    SetVisible(mUI.btnDelTextureMap,  false);
    SetVisible(mUI.btnAddTextureMap,  false);
    SetVisible(mUI.chkBlendFrames,    false);

    SetValue(mUI.materialType, mMaterial->GetType());
    SetValue(mUI.materialID, mMaterial->GetId());
    SetValue(mUI.shaderFile,  QString(""));
    SetValue(mUI.textureName, QString(""));
    SetValue(mUI.gamma, 1.0f);
    SetValue(mUI.baseColor, gfx::Color::White);
    SetValue(mUI.spriteFps, 1.0f);
    SetValue(mUI.chkStaticInstance, false);
    SetValue(mUI.chkBlendFrames,    false);
    SetValue(mUI.colorMap0, gfx::Color::White);
    SetValue(mUI.colorMap1, gfx::Color::White);
    SetValue(mUI.colorMap2, gfx::Color::White);
    SetValue(mUI.colorMap3, gfx::Color::White);
    SetValue(mUI.gradientOffsetX, NormalizedFloat(0.5f));
    SetValue(mUI.gradientOffsetY, NormalizedFloat(0.5f));
    SetValue(mUI.scaleX,    1.0f);
    SetValue(mUI.scaleY,    1.0f);
    SetValue(mUI.velocityX, 1.0f);
    SetValue(mUI.velocityY, 1.0f);
    SetValue(mUI.velocityZ, 1.0f);
    SetValue(mUI.minFilter, gfx::MaterialClass::MinTextureFilter::Default);
    SetValue(mUI.magFilter, gfx::MaterialClass::MagTextureFilter::Default);
    SetValue(mUI.wrapX,     gfx::MaterialClass::TextureWrapping::Clamp);
    SetValue(mUI.wrapY,     gfx::MaterialClass::TextureWrapping::Clamp);
    ClearList(mUI.textures);

    SetEnabled(mUI.textureProp,    false);
    SetEnabled(mUI.texturePreview, false);
    SetEnabled(mUI.textureRect,    false);
    SetValue(mUI.textureFile,   QString(""));
    SetValue(mUI.textureID,     QString(""));
    SetValue(mUI.textureWidth,  QString(""));
    SetValue(mUI.textureHeight, QString(""));
    SetValue(mUI.textureDepth,  QString(""));
    SetValue(mUI.rectX, 0.0f);
    SetValue(mUI.rectY, 0.0f);
    SetValue(mUI.rectW, 1.0f);
    SetValue(mUI.rectH, 1.0f);
    mUI.texturePreview->setPixmap(QPixmap(":texture.png"));

    if (auto* ptr = mMaterial->AsBuiltIn())
    {
        SetVisible(mUI.builtInProperties, true);

        SetEnabled(mUI.gamma,             true);
        SetEnabled(mUI.chkStaticInstance, true);
        SetValue(mUI.chkStaticInstance, ptr->IsStatic());
        SetValue(mUI.surfaceType,       ptr->GetSurfaceType());
        SetValue(mUI.gamma,             ptr->GetGamma());
    }

    if (auto* ptr = mMaterial->AsCustom())
    {
        SetVisible(mUI.customUniforms, true);
        SetVisible(mUI.customSamplers, true);
        SetVisible(mUI.textureFilters, true);
        SetVisible(mUI.textureMaps,      true);
        SetEnabled(mUI.shaderFile,      true);
        SetEnabled(mUI.btnReloadShader, true);
        SetEnabled(mUI.btnSelectShader, true);
        SetEnabled(mUI.textureFilters,  true);
        SetEnabled(mUI.textureMaps,     true);
        SetValue(mUI.shaderFile, ptr->GetShaderUri());
        SetValue(mUI.surfaceType, ptr->GetSurfaceType());
        SetValue(mUI.minFilter, ptr->GetTextureMinFilter());
        SetValue(mUI.magFilter, ptr->GetTextureMagFilter());
        SetValue(mUI.wrapX, ptr->GetTextureWrapX());
        SetValue(mUI.wrapY, ptr->GetTextureWrapY());

        for (auto* widget : mUniforms)
        {
            const auto& name = app::ToUtf8(widget->GetName());
            if (const auto* val = ptr->GetUniformValue<float>(name))
                widget->SetValue(*val);
            else if (const auto* val = ptr->GetUniformValue<glm::vec2>(name))
                widget->SetValue(*val);
            else if (const auto* val = ptr->GetUniformValue<glm::vec3>(name))
                widget->SetValue(*val);
            else if (const auto* val = ptr->GetUniformValue<glm::vec4>(name))
                widget->SetValue(*val);
            else if (const auto* val = ptr->GetUniformValue<gfx::Color4f>(name))
                widget->SetValue(FromGfx(*val));
            else BUG("No such uniform in material. UI and material are out of sync.");
        }

        std::vector<ListItem> textures;
        for (auto* widget : mSamplers)
        {
            widget->SetText("");
            const auto& name = app::ToUtf8(widget->GetName());
            const auto* map  = ptr->FindTextureMap(name);
            if (const auto* sprite = map->AsSpriteMap())
            {
                for (size_t i=0; i<sprite->GetNumTextures(); ++i)
                {
                    if (const auto* source = sprite->GetTextureSource(i))
                    {
                        ListItem item;
                        item.id   = app::FromUtf8(source->GetId());
                        item.name = app::FromUtf8(source->GetName());
                        textures.push_back(item);
                    }
                }
                if (sprite->GetNumTextures())
                    widget->SetText(QString("%1 texture(s)").arg(sprite->GetNumTextures()));
                widget->SetSpriteFps(sprite->GetFps());
            }
            else if (const auto* texture = map->AsTextureMap2D())
            {
                if (const auto* source = texture->GetTextureSource())
                {
                    ListItem item;
                    item.id   = app::FromUtf8(source->GetId());
                    item.name = app::FromUtf8(source->GetName());
                    textures.push_back(item);
                    widget->SetText(app::FromUtf8(source->GetName()));
                }
            } else BUG("Texture map not found in material.");
        }
        SetList(mUI.textures, textures);
        if (!textures.empty() && mUI.textures->currentRow() == -1)
            mUI.textures->setCurrentRow(0);

        SetVisible(mUI.textureFilters, !textures.empty());

        GetTextureProperties();
    }
    else if (auto* ptr = mMaterial->AsColor())
    {
        SetEnabled(mUI.baseColor,   true);
        SetVisible(mUI.baseColor,    true);
        SetVisible(mUI.lblBaseColor, true);
        SetValue(mUI.baseColor, ptr->GetBaseColor());

    }
    else if (auto* ptr = mMaterial->AsGradient())
    {
        const auto& offset = ptr->GetOffset();
        SetVisible(mUI.gradientMap, true);
        SetEnabled(mUI.gradientMap, true);
        SetEnabled(mUI.gradientOffsetY, true);
        SetEnabled(mUI.gradientOffsetX, true);
        SetVisible(mUI.gradientOffsetX, true);
        SetVisible(mUI.gradientOffsetY, true);
        SetValue(mUI.colorMap0, ptr->GetColor(gfx::GradientClass::ColorIndex::TopLeft));
        SetValue(mUI.colorMap1, ptr->GetColor(gfx::GradientClass::ColorIndex::TopRight));
        SetValue(mUI.colorMap2, ptr->GetColor(gfx::GradientClass::ColorIndex::BottomLeft));
        SetValue(mUI.colorMap3, ptr->GetColor(gfx::GradientClass::ColorIndex::BottomRight));
        SetValue(mUI.gradientOffsetY, NormalizedFloat(offset.y));
        SetValue(mUI.gradientOffsetX, NormalizedFloat(offset.x));

    }
    else if (auto* ptr = mMaterial->AsTexture())
    {
        SetVisible(mUI.baseColor,         true);
        SetVisible(mUI.lblBaseColor,      true);
        SetVisible(mUI.particleAction,    true);
        SetVisible(mUI.lblParticleEffect, true);
        SetVisible(mUI.lblTexture,        true);
        SetVisible(mUI.textureName,       true);
        SetVisible(mUI.btnDelTextureMap,  true);
        SetVisible(mUI.btnAddTextureMap,  true);
        SetVisible(mUI.textureCoords,     true);
        SetVisible(mUI.textureFilters,    true);
        SetEnabled(mUI.baseColor,         true);
        SetEnabled(mUI.btnAddTextureMap,  true);
        SetEnabled(mUI.particleAction,    true);
        SetEnabled(mUI.textureCoords,     true);
        SetEnabled(mUI.textureFilters,    true);
        SetEnabled(mUI.textureMaps,       true);

        SetValue(mUI.baseColor, ptr->GetBaseColor());
        SetValue(mUI.minFilter, ptr->GetTextureMinFilter());
        SetValue(mUI.magFilter, ptr->GetTextureMagFilter());
        SetValue(mUI.scaleX,    ptr->GetTextureScaleX());
        SetValue(mUI.scaleY,    ptr->GetTextureScaleY());
        SetValue(mUI.wrapX,     ptr->GetTextureWrapX());
        SetValue(mUI.wrapY,     ptr->GetTextureWrapY());
        SetValue(mUI.velocityX, ptr->GetTextureVelocityX());
        SetValue(mUI.velocityY, ptr->GetTextureVelocityY());
        SetValue(mUI.velocityZ, ptr->GetTextureVelocityZ());

        std::vector<ListItem> textures;
        if (auto* source = ptr->GetTextureSource())
        {
            ListItem item;
            item.id   = app::FromUtf8(source->GetId());
            item.name = app::FromUtf8(source->GetName());
            textures.push_back(item);
            SetValue(mUI.textureName, source->GetName());
            SetEnabled(mUI.btnDelTextureMap, true);
        }
        SetList(mUI.textures, textures);
        if (!textures.empty() && mUI.textures->currentRow() == -1)
            mUI.textures->setCurrentRow(0);
        GetTextureProperties();
    }
    else if (auto* ptr = mMaterial->AsSprite())
    {
        SetVisible(mUI.baseColor,         true);
        SetVisible(mUI.lblBaseColor,      true);
        SetVisible(mUI.particleAction,    true);
        SetVisible(mUI.lblParticleEffect, true);
        SetVisible(mUI.lblSpriteFps,      true);
        SetVisible(mUI.spriteFps,         true);
        SetVisible(mUI.lblTexture,        true);
        SetVisible(mUI.textureName,       true);
        SetVisible(mUI.btnDelTextureMap,  true);
        SetVisible(mUI.btnAddTextureMap,  true);
        SetVisible(mUI.textureCoords,     true);
        SetVisible(mUI.textureFilters,    true);
        SetVisible(mUI.chkBlendFrames,    true);
        SetEnabled(mUI.baseColor,        true);
        SetEnabled(mUI.spriteFps,        true);
        SetEnabled(mUI.particleAction,   true);
        SetEnabled(mUI.textureCoords,    true);
        SetEnabled(mUI.textureFilters,   true);
        SetEnabled(mUI.textureMaps,      true);
        SetEnabled(mUI.chkBlendFrames,   true);
        SetEnabled(mUI.btnAddTextureMap, true);

        SetValue(mUI.baseColor, ptr->GetBaseColor());
        SetValue(mUI.minFilter, ptr->GetTextureMinFilter());
        SetValue(mUI.magFilter, ptr->GetTextureMagFilter());
        SetValue(mUI.scaleX,    ptr->GetTextureScaleX());
        SetValue(mUI.scaleY,    ptr->GetTextureScaleY());
        SetValue(mUI.wrapX,     ptr->GetTextureWrapX());
        SetValue(mUI.wrapY,     ptr->GetTextureWrapY());
        SetValue(mUI.velocityX, ptr->GetTextureVelocityX());
        SetValue(mUI.velocityY, ptr->GetTextureVelocityY());
        SetValue(mUI.velocityZ, ptr->GetTextureVelocityZ());
        SetValue(mUI.spriteFps, ptr->GetFps());
        SetValue(mUI.chkBlendFrames, ptr->GetBlendFrames());

        std::vector<ListItem> textures;
        for (size_t i=0; i<ptr->GetNumTextures(); ++i)
        {
            const auto* source = ptr->GetTextureSource(i);
            ListItem item;
            item.id   = app::FromUtf8(source->GetId());
            item.name = app::FromUtf8(source->GetName());
            textures.push_back(item);
            SetEnabled(mUI.btnDelTextureMap, true);
        }
        SetList(mUI.textures, textures);
        if (!textures.empty() && mUI.textures->currentRow() == -1)
            mUI.textures->setCurrentRow(0);

        if (!textures.empty())
            SetValue(mUI.textureName, QString("%1 texture(s)").arg(textures.size()));

        GetTextureProperties();
    }
}

void MaterialWidget::GetTextureProperties()
{
    SetEnabled(mUI.textureProp,    false);
    SetEnabled(mUI.texturePreview, false);
    SetEnabled(mUI.textureRect,    false);
    SetValue(mUI.textureFile,   QString(""));
    SetValue(mUI.textureID,     QString(""));
    SetValue(mUI.textureWidth,  QString(""));
    SetValue(mUI.textureHeight, QString(""));
    SetValue(mUI.textureDepth,  QString(""));
    SetValue(mUI.rectX, 0.0f);
    SetValue(mUI.rectY, 0.0f);
    SetValue(mUI.rectW, 1.0f);
    SetValue(mUI.rectH, 1.0f);
    mUI.texturePreview->setPixmap(QPixmap(":texture.png"));

    const auto current = mUI.textures->currentRow();
    if (current == -1)
        return;

    gfx::TextureSource* source = nullptr;
    gfx::FRect rect;
    if (auto* texture = mMaterial->AsTexture())
    {
        source = texture->GetTextureSource();
        rect   = texture->GetTextureRect();
    }
    else if (auto* sprite = mMaterial->AsSprite())
    {
        source = sprite->GetTextureSource(current);
        rect   = sprite->GetTextureRect(current);
    }
    else if (auto* custom = mMaterial->AsCustom())
    {
        source = custom->FindTextureSourceById(GetItemId(mUI.textures));
        rect   = custom->FindTextureSourceRect(source);
    } else BUG("Unhandled material type with textures.");

    ASSERT(source);

    SetEnabled(mUI.textureProp,    true);
    SetEnabled(mUI.texturePreview, true);
    SetEnabled(mUI.textureRect,    true);

    if (const auto bitmap = source->GetData())
    {
        const auto width  = bitmap->GetWidth();
        const auto height = bitmap->GetHeight();
        const auto depth  = bitmap->GetDepthBits();
        QImage img;
        if (depth == 8)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width, QImage::Format_Grayscale8);
        else if (depth == 24)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
        else if (depth == 32)
            img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
        else ERROR_RETURN("Failed to load texture preview. Unexpected bit depth %1", depth);

        QPixmap pix;
        pix.convertFromImage(img);
        SetValue(mUI.textureWidth,  width);
        SetValue(mUI.textureHeight, height);
        SetValue(mUI.textureDepth,  depth);
        mUI.texturePreview->setPixmap(pix.scaledToHeight(128));
    }
    else ERROR_RETURN("Failed to load texture preview.");

    SetValue(mUI.rectX,     rect.GetX());
    SetValue(mUI.rectY,     rect.GetY());
    SetValue(mUI.rectW,     rect.GetWidth());
    SetValue(mUI.rectH,     rect.GetHeight());
    SetValue(mUI.textureID, source->GetId());
    SetValue(mUI.textureFile, QString("N/A"));
    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(source))
    {
        SetValue(mUI.textureFile, ptr->GetFilename());
    }
}

void MaterialWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto zoom = mUI.zoom->value();
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5f;
    const auto ypos = (height - content_height) * 0.5f;
    const auto drawable = mWorkspace->MakeDrawableByName(GetValue(mUI.cmbModel));

    // use the material to fill a model shape in the middle of the screen.
    gfx::Transform transform;
    transform.MoveTo(xpos, ypos);
    transform.Resize(content_width, content_height);

    std::string message;
    bool dummy   = false;
    bool success = true;

    if (const auto* texture = mMaterial->AsTexture())
    {
        if (!texture->GetTextureSource())
        {
            message = "Texture map is not set.";
            dummy   = true;
            success = false;
        }
    }
    else if (const auto* sprite = mMaterial->AsSprite())
    {
        if (sprite->GetNumTextures() == 0)
        {
            message = "Sprite texture maps are not set.";
            dummy   = true;
            success = false;
        }
    }
    else if (const auto* custom = mMaterial->AsCustom())
    {
        const auto& uri = custom->GetShaderUri();
        if (uri.empty())
        {
            message = "No shader has been selected.";
            success = false;
        }
        const auto& map_names = custom->GetTextureMapNames();
        for (const auto& name : map_names)
        {
            const auto* map = custom->FindTextureMap(name);
            if (const auto* texture = map->AsTextureMap2D())
            {
                if (!texture->GetTextureSource())
                {
                    message = base::FormatString("Texture map '%1' is not set.", name);
                    success = false;
                    dummy   = true;
                }
            }
            else if (const auto* sprite = map->AsSpriteMap())
            {
                if (sprite->GetNumTextures() == 0)
                {
                    message = base::FormatString("Sprite map '%1' is not set.", name);
                    success = false;
                    dummy   = true;
                }
            }
        }
    }
    if (!success)
    {
        if (dummy)
        {
            gfx::TextureMap2DClass dummy;
            dummy.SetTexture(gfx::LoadTextureFromFile("app://textures/Checkerboard.png"));
            painter.Draw(*drawable, transform, dummy);
        }
        ShowMessage(message, painter, width, height);
        return;
    }

    gfx::Material material(mMaterial);
    material.SetRuntime(mTime);
    painter.Draw(*drawable, transform, material);
}

} // namespace

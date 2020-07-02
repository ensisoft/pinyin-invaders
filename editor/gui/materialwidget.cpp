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

#define LOGTAG "material"

#include "config.h"

#include "warnpush.h"
#  include <QMessageBox>
#  include <QFileDialog>
#  include <QFileInfo>
#  include <QPixmap>
#  include <QTextStream>
#  include <base64/base64.h>
#include "warnpop.h"

#include "base/assert.h"
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
#include "materialwidget.h"

namespace gui
{

MaterialWidget::MaterialWidget(app::Workspace* workspace)
{
    mWorkspace = workspace;

    DEBUG("Create MaterialWidget");

    mUI.setupUi(this);
    mUI.widget->setFramerate(60);
    mUI.widget->onPaintScene = std::bind(&MaterialWidget::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.actionPause->setEnabled(false);
    mUI.actionPlay->setEnabled(true);
    mUI.actionStop->setEnabled(false);
    mUI.materialName->setText("My Material");
    setWindowTitle("My Material");

    PopulateFromEnum<gfx::Material::MinTextureFilter>(mUI.minFilter);
    PopulateFromEnum<gfx::Material::MagTextureFilter>(mUI.magFilter);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapX);
    PopulateFromEnum<gfx::Material::TextureWrapping>(mUI.wrapY);
    PopulateFromEnum<gfx::Material::SurfaceType>(mUI.surfaceType);
    PopulateFromEnum<gfx::Material::Type>(mUI.materialType);
}

MaterialWidget::MaterialWidget(app::Workspace* workspace, const app::Resource& resource) : MaterialWidget(workspace)
{
    DEBUG("Editing material: '%1'", resource.GetName());

    mMaterial = *resource.GetContent<gfx::Material>();

    SetValue(mUI.materialType, mMaterial.GetType());
    SetValue(mUI.surfaceType,  mMaterial.GetSurfaceType());
    SetValue(mUI.minFilter,    mMaterial.GetMinTextureFilter());
    SetValue(mUI.magFilter,    mMaterial.GetMagTextureFilter());
    SetValue(mUI.wrapX,        mMaterial.GetTextureWrapX());
    SetValue(mUI.wrapY,        mMaterial.GetTextureWrapY());
    SetValue(mUI.baseColor,    mMaterial.GetBaseColor());
    SetValue(mUI.scaleX,       mMaterial.GetTextureScaleX());
    SetValue(mUI.scaleY,       mMaterial.GetTextureScaleY());
    SetValue(mUI.fps,          mMaterial.GetFps());
    SetValue(mUI.blend,        mMaterial.GetBlendFrames());
    SetValue(mUI.gamma,        mMaterial.GetGamma());

    size_t num_textures = mMaterial.GetNumTextures();
    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& source = mMaterial.GetTextureSource(i);
        if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(&source))
        {
            if (MissingFile(ptr->GetFilename()))
            {
                WARN("Texture file '%1' could no longer be found.", ptr->GetFilename());
                // todo: we're skipping adding this item to the list because
                // the code in gfx::material  will expect the file to be available
                // or it will throw, perhaps that code should be using logging instead
                // and we should leave the item in the list widget =
                continue;
            }
        }
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(app::FromUtf8(source.GetName()));
        item->setData(Qt::UserRole, app::FromUtf8(source.GetId()));
        mUI.textures->addItem(item);
    }

    if (MissingFile(mUI.shaderFile))
    {
        WARN("The shader file '%1' could no longer be found.", mUI.shaderFile->text());
    }

    mUI.textures->setCurrentRow(0);
    mUI.materialName->setText(resource.GetName());
    setWindowTitle(resource.GetName());

    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");
}

MaterialWidget::~MaterialWidget()
{
    DEBUG("Destroy MaterialWidget");

    mUI.widget->dispose();
}

void MaterialWidget::addActions(QToolBar& bar)
{
    bar.addAction(mUI.actionPlay);
    bar.addAction(mUI.actionPause);
    bar.addSeparator();
    bar.addAction(mUI.actionStop);
    bar.addSeparator();
    bar.addAction(mUI.actionSave);
}
void MaterialWidget::addActions(QMenu& menu)
{
    menu.addAction(mUI.actionPlay);
    menu.addAction(mUI.actionPause);
    menu.addSeparator();
    menu.addAction(mUI.actionStop);
    menu.addSeparator();
    menu.addAction(mUI.actionSave);
}

bool MaterialWidget::loadState(const Settings& settings)
{
    const std::string& base64 = settings.getValue("Material", "content", std::string(""));
    if (base64.empty())
        return true;

    const auto& json = nlohmann::json::parse(base64::Decode(base64));
    auto ret = gfx::Material::FromJson(json);
    if (!ret.has_value())
    {
        WARN("Failed to load material widget state.");
        return false;
    }
    mMaterial = std::move(ret.value());

    // restore UI from the material values.
    SetValue(mUI.materialType, mMaterial.GetType());
    SetValue(mUI.surfaceType,  mMaterial.GetSurfaceType());
    SetValue(mUI.minFilter,    mMaterial.GetMinTextureFilter());
    SetValue(mUI.magFilter,    mMaterial.GetMagTextureFilter());
    SetValue(mUI.wrapX,        mMaterial.GetTextureWrapX());
    SetValue(mUI.wrapY,        mMaterial.GetTextureWrapY());
    SetValue(mUI.baseColor,    mMaterial.GetBaseColor());
    SetValue(mUI.scaleX,       mMaterial.GetTextureScaleX());
    SetValue(mUI.scaleY,       mMaterial.GetTextureScaleY());
    SetValue(mUI.fps,          mMaterial.GetFps());
    SetValue(mUI.blend,        mMaterial.GetBlendFrames());
    SetValue(mUI.gamma,        mMaterial.GetGamma());

    // restore textures list
    size_t num_textures = mMaterial.GetNumTextures();
    for (size_t i=0; i<num_textures; ++i)
    {
        const auto& source = mMaterial.GetTextureSource(i);
        if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(&source))
        {
            if (MissingFile(ptr->GetFilename()))
            {
                WARN("Texture file '%1' could no longer be found.", ptr->GetFilename());
                // todo: we're skipping adding this item to the list because
                // the code in gfx::material  will expect the file to be available
                // or it will throw, perhaps that code should be using logging instead
                // and we should leave the item in the list widget =
                continue;
            }
        }
        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(app::FromUtf8(source.GetName()));
        item->setData(Qt::UserRole, app::FromUtf8(source.GetId()));
        mUI.textures->addItem(item);
    }

    // select first texture if any
    mUI.textures->setCurrentRow(0);
    // enable/disable UI elements based on the material type
    on_materialType_currentIndexChanged("");

    settings.loadWidget("Material", mUI.materialName);
    settings.loadWidget("Material", mUI.shaderFile);
    settings.loadWidget("Material", mUI.customShader);
    settings.loadWidget("Material", mUI.zoom);

    if (MissingFile(mUI.shaderFile))
    {
        WARN("The shader file '%1' could no longer be found.", mUI.shaderFile->text());
    }

    setWindowTitle(mUI.materialName->text());
    return true;
}

bool MaterialWidget::saveState(Settings& settings) const
{
    settings.saveWidget("Material", mUI.materialName);
    settings.saveWidget("Material", mUI.shaderFile);
    settings.saveWidget("Material", mUI.customShader);
    settings.saveWidget("Material", mUI.zoom);

    SetMaterialProperties();

    // the material can already serialize into JSON.
    // so let's use the material's JSON serialization here as well.
    const auto& json = mMaterial.ToJson();
    const auto& base64 = base64::Encode(json.dump(2));
    settings.setValue("Material", "content", base64);
    return true;
}

void MaterialWidget::zoomIn()
{
    const auto value = mUI.zoom->value();
    mUI.zoom->setValue(value + 0.1);
}

void MaterialWidget::zoomOut()
{
    const auto value = mUI.zoom->value();
    if (value > 0.1)
        mUI.zoom->setValue(value - 0.1);
}

void MaterialWidget::reloadShaders()
{
    mUI.widget->reloadShaders();
}
void MaterialWidget::reloadTextures()
{
    mUI.widget->reloadTextures();
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
    const auto& name = mUI.materialName->text();
    if (name.isEmpty())
    {
        mUI.materialName->setFocus();
        return;
    }
    if (mWorkspace->HasMaterial(name))
    {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Question);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setText(tr("Workspace already contains a material by this name. Overwrite ?"));
        if (msg.exec() == QMessageBox::No)
            return;
    }

    SetMaterialProperties();

    app::MaterialResource resource(mMaterial, name);
    resource.SetProperty("shader_file", mUI.shaderFile->text());
    resource.SetProperty("use_shader_file", mUI.customShader->isChecked());

    mWorkspace->SaveResource(resource);

    setWindowTitle(name);

    NOTE("Saved material '%1'", name);
}

void MaterialWidget::on_btnAddTextureMap_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Image File(s)"), "", tr("Images (*.png *.jpg *.jpeg)"));
    if (list.isEmpty())
        return;

    for (const auto& file : list)
    {
        QFileInfo info(file);
        const auto& name = info.baseName();
        const auto& path = info.absoluteFilePath();

        auto source = std::make_shared<gfx::detail::TextureFileSource>(app::ToUtf8(path), app::ToUtf8(name));

        QListWidgetItem* item = new QListWidgetItem(mUI.textures);
        item->setText(name);
        item->setData(Qt::UserRole, app::FromUtf8(source->GetId()));
        mUI.textures->addItem(item);

        mMaterial.AddTexture(source);
    }
    const auto index = mUI.textures->currentRow();
    mUI.textures->setCurrentRow(index == -1 ? 0 : index);
}

void MaterialWidget::on_btnDelTextureMap_clicked()
{
    // make sure currentRowChanged is blocked while we're deleting shit.
    QSignalBlocker cockblocker(mUI.textures);

    QList<QListWidgetItem*> items = mUI.textures->selectedItems();

    // a bit ugly but ok.
    for (int i=0; i<items.count(); ++i)
    {
        const QListWidgetItem* item = items[i];
        const auto& data = item->data(Qt::UserRole).toString();
        const auto& id   = app::ToUtf8(data);
        const auto num_textures = mMaterial.GetNumTextures();
        bool deleted = false;
        for (size_t i=0; i<num_textures; ++i)
        {
            if (mMaterial.GetTextureSource(i).GetId() != id)
                continue;

            mMaterial.DeleteTexture(i);
            deleted = true;
            break;
        }
        ASSERT(deleted);
    }

    qDeleteAll(items);

    const auto current = mUI.textures->currentRow();

    on_textures_currentRowChanged(current);
}

void MaterialWidget::on_btnEditTextureMap_clicked()
{
    const auto item = mUI.textures->currentRow();
    if (item == -1)
        return;

    const auto& source = mMaterial.GetTextureSource(item);
    if (const auto* ptr = dynamic_cast<const gfx::detail::TextureFileSource*>(&source))
    {
        emit openExternalImage(app::FromUtf8(ptr->GetFilename()));
    }
}

void MaterialWidget::on_browseShader_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Shader File"), "", tr("GLSL files (*.glsl)"));
    if (list.isEmpty())
        return;

    mUI.shaderFile->setText(list[0]);
    mUI.shaderFile->setCursorPosition(0);
}

void MaterialWidget::on_textures_currentRowChanged(int index)
{
    if (index == -1)
    {
        mUI.textureRect->setEnabled(false);
        mUI.textureProp->setEnabled(false);
        mUI.textureWidth->setText("");
        mUI.textureHeight->setText("");
        mUI.textureDepth->setText("");
        mUI.btnDelTextureMap->setEnabled(false);
        mUI.texturePreview->setPixmap(QPixmap(":texture.png"));
        return;
    }

    const auto& source = mMaterial.GetTextureSource(index);
    const auto& rect   = mMaterial.GetTextureRect(index);
    const auto& bitmap = source.GetData();

    const auto width  = bitmap->GetWidth();
    const auto height = bitmap->GetHeight();
    const auto depth  = bitmap->GetDepthBits();
    QImage img;
    if (depth == 8)
        img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, height, QImage::Format_Grayscale8);
    else if (depth == 24)
        img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 3, QImage::Format_RGB888);
    else if (depth == 32)
        img = QImage((const uchar*)bitmap->GetDataPtr(), width, height, width * 4, QImage::Format_RGBA8888);
    else throw std::runtime_error("unexpected image depth: " + std::to_string(depth));

    QPixmap pix;
    pix.convertFromImage(img);

    mUI.textureWidth->setText(QString::number(width));
    mUI.textureHeight->setText(QString::number(height));
    mUI.textureDepth->setText(QString::number(depth));
    mUI.texturePreview->setPixmap(pix.scaledToHeight(128));
    mUI.rectX->setValue(rect.GetX());
    mUI.rectY->setValue(rect.GetY());
    mUI.rectW->setValue(rect.GetWidth());
    mUI.rectH->setValue(rect.GetHeight());

    mUI.textureProp->setEnabled(true);
    mUI.textureRect->setEnabled(true);
    mUI.btnDelTextureMap->setEnabled(true);
}

void MaterialWidget::on_materialType_currentIndexChanged(const QString& text)
{
    mUI.texturing->setEnabled(false);
    mUI.animation->setEnabled(false);
    mUI.textureMaps->setEnabled(false);
    mUI.textureProp->setEnabled(false);
    mUI.textureRect->setEnabled(false);

    // enable/disable UI properties based on the
    // material type.
    const gfx::Material::Type type = GetValue(mUI.materialType);

    if (type == gfx::Material::Type::Color)
    {
        mUI.baseProperties->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Texture)
    {
        mUI.texturing->setEnabled(true);
        mUI.textureMaps->setEnabled(true);
        mUI.textureProp->setEnabled(true);
        mUI.textureRect->setEnabled(true);
    }
    else if (type == gfx::Material::Type::Sprite)
    {
        mUI.texturing->setEnabled(true);
        mUI.textureMaps->setEnabled(true);
        mUI.textureProp->setEnabled(true);
        mUI.textureRect->setEnabled(true);
        mUI.animation->setEnabled(true);
    }
}

void MaterialWidget::on_rectX_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetX(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectY_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetY(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectW_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetWidth(value);
    mMaterial.SetTextureRect(row, rect);
}
void MaterialWidget::on_rectH_valueChanged(double value)
{
    const auto row = mUI.textures->currentRow();
    if (row == -1)
        return;

    gfx::FRect rect = mMaterial.GetTextureRect(row);
    rect.SetHeight(value);
    mMaterial.SetTextureRect(row, rect);
}

void MaterialWidget::SetMaterialProperties() const
{
    if (mUI.customShader->isChecked())
    {
        const QString& file = mUI.shaderFile->text();
        mMaterial.SetShaderFile(app::ToUtf8(file));
    }
    else
    {
        mMaterial.SetShaderFile("");
    }

    mMaterial.SetType(GetValue(mUI.materialType));
    mMaterial.SetBaseColor(GetValue(mUI.baseColor));
    mMaterial.SetGamma(GetValue(mUI.gamma));
    mMaterial.SetSurfaceType(GetValue(mUI.surfaceType));
    mMaterial.SetRuntime(mTime);
    mMaterial.SetFps(GetValue(mUI.fps));
    mMaterial.SetBlendFrames(GetValue(mUI.blend));
    mMaterial.SetTextureMinFilter(GetValue(mUI.minFilter));
    mMaterial.SetTextureMagFilter(GetValue(mUI.magFilter));
    mMaterial.SetTextureScaleX(GetValue(mUI.scaleX));
    mMaterial.SetTextureScaleY(GetValue(mUI.scaleY));
    mMaterial.SetTextureWrapX(GetValue(mUI.wrapX));
    mMaterial.SetTextureWrapY(GetValue(mUI.wrapY));
}

void MaterialWidget::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    SetMaterialProperties();

    const auto zoom = mUI.zoom->value();
    const auto content_width  = width * zoom;
    const auto content_height = width * zoom;
    const auto xpos = (width - content_width) * 0.5f;
    const auto ypos = (height - content_height) * 0.5f;

    // use the material to fill a rectangle in the middle of the screen.
    gfx::FillRect(painter, gfx::FRect(xpos, ypos, content_width, content_height), mMaterial);

    if (mState == PlayState::Playing)
    {
        const unsigned wtf = secs * 1000;
        mTime += (wtf / 1000.0f);
    }
    mUI.time->setText(QString::number(mTime));
}

} // namespace

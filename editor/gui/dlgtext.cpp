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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"
#  include <QCoreApplication>
#  include <QFileDialog>
#  include <QFileInfo>
#include "warnpop.h"

#include "editor/gui/utility.h"
#include "graphics/text.h"
#include "graphics/drawing.h"
#include "graphics/material.h"
#include "dlgtext.h"

namespace gui
{

DlgText::DlgText(QWidget* parent, gfx::TextBuffer& text)
    : QDialog(parent)
    , mText(text)
{
    mUI.setupUi(this);
    PopulateFromEnum<gfx::TextBuffer::HorizontalAlignment>(mUI.cmbHAlign);
    PopulateFromEnum<gfx::TextBuffer::VerticalAlignment>(mUI.cmbVAlign);

    mUI.widget->onPaintScene = std::bind(&DlgText::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);

    QStringList filters;
    filters << "*.ttf" << "*.otf";
    const auto& appdir  = QCoreApplication::applicationDirPath();
    const auto& fontdir = app::JoinPath(appdir, "fonts");
    QDir dir;
    dir.setPath(fontdir);
    dir.setNameFilters(filters);
    const QStringList& files = dir.entryList();
    for (const auto& file : files) {
        const QFileInfo info(file);
        mUI.cmbFont->addItem("app://fonts/" + info.fileName());
    }

    if (!text.IsEmpty())
    {
        const auto& text_and_style = text.GetText(0);
        //SetValue(mUI.cmbFont, text_and_style.font);
        mUI.cmbFont->setCurrentText(app::FromUtf8(text_and_style.font));
        SetValue(mUI.fontSize, text_and_style.fontsize);
        SetValue(mUI.underline, text_and_style.underline);
        SetValue(mUI.lineHeight, text_and_style.lineheight);
        SetValue(mUI.cmbVAlign, text_and_style.valign);
        SetValue(mUI.cmbHAlign, text_and_style.halign);
        SetValue(mUI.text, text_and_style.text);
    }
    SetValue(mUI.bufferWidth, text.GetWidth());
    SetValue(mUI.bufferHeight, text.GetHeight());

    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, this, &DlgText::finished);

    connect(&mTimer, &QTimer::timeout, this, &DlgText::timer);
    mTimer.setInterval(1000.0/60.0f);
    mTimer.start();
}

void DlgText::on_btnAccept_clicked()
{
    accept();
}
void DlgText::on_btnCancel_clicked()
{
    reject();
}
void DlgText::on_btnFont_clicked()
{
    const auto& list = QFileDialog::getOpenFileNames(this,
        tr("Select Font File"), "", tr("Font (*.ttf *.otf)"));
    if (list.isEmpty())
        return;

    mUI.cmbFont->setCurrentText(list[0]);
}

void DlgText::finished()
{
    mUI.widget->dispose();
}

void DlgText::timer()
{
    mUI.widget->triggerPaint();
}

void DlgText::PaintScene(gfx::Painter& painter, double secs)
{
    const auto widget_width = mUI.widget->width();
    const auto widget_height = mUI.widget->height();
    painter.SetViewport(0, 0, widget_width, widget_height);

    const QString& text = GetValue(mUI.text);
    const QString& font = GetValue(mUI.cmbFont);
    if (text.isEmpty() || font.isEmpty())
        return;

    const unsigned buffer_width  = GetValue(mUI.bufferWidth);
    const unsigned buffer_height = GetValue(mUI.bufferHeight);

    gfx::TextBuffer::Text text_and_style;
    text_and_style.text       = app::ToUtf8(text);
    text_and_style.font       = app::ToUtf8(font);
    text_and_style.fontsize   = GetValue(mUI.fontSize);
    text_and_style.underline  = GetValue(mUI.underline);
    text_and_style.valign     = GetValue(mUI.cmbVAlign);
    text_and_style.halign     = GetValue(mUI.cmbHAlign);
    text_and_style.lineheight = GetValue(mUI.lineHeight);

    mText.SetSize(buffer_width, buffer_height);
    mText.ClearText();
    mText.AddText(std::move(text_and_style));

    gfx::Material material;
    material.SetType(gfx::Material::Type::Texture);
    material.SetSurfaceType(gfx::Material::SurfaceType::Transparent);
    material.SetBaseColor(gfx::Color::White);
    material.AddTexture(mText);

    if ((bool)GetValue(mUI.chkScale))
    {
        const auto scale = std::min((float)widget_width / (float)buffer_width,
                                    (float)widget_height / (float)buffer_height);
        const auto render_width = buffer_width * scale;
        const auto render_height = buffer_height * scale;
        const auto x = (widget_width - render_width) / 2.0f;
        const auto y = (widget_height - render_height) / 2.0f;
        gfx::FillRect(painter, gfx::Rect(x, y, render_width, render_height), material);
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, render_width, render_height),
            gfx::SolidColor(gfx::Color::DarkGreen), 1.0f);
    }
    else
    {
        const auto x = (widget_width - buffer_width) / 2;
        const auto y = (widget_height - buffer_height) / 2;
        gfx::FillRect(painter, gfx::FRect(x, y, buffer_width, buffer_height), material);
        gfx::DrawRectOutline(painter, gfx::FRect(x, y, buffer_width, buffer_height),
            gfx::SolidColor(gfx::Color::DarkGreen), 1.0f);
    }
}

} // namespace

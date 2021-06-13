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

#include "config.h"

#include <cmath>

#include "base/assert.h"
#include "base/utility.h"
#include "graphics/bitmap.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"
#include "graphics/text.h"

namespace {
gfx::Material MakeMaterial(const gfx::Color4f& color)
{
    const auto alpha = color.Alpha();
    auto mat = gfx::CreateMaterialFromColor(color);
    mat.SetSurfaceType(alpha == 1.0f
                       ? gfx::MaterialClass::SurfaceType::Opaque
                       : gfx::MaterialClass::SurfaceType::Transparent);
    return mat;
}
} // namespace

namespace gfx
{

void DrawTextRect(Painter& painter,
    const std::string& text,
    const std::string& font,
    unsigned font_size_px,
    const FRect& rect,
    const Color4f& color,
    unsigned alignment,
    unsigned properties,
    float line_height)
{
    TextBuffer buff(rect.GetWidth(), rect.GetHeight());
    if ((alignment & 0xf) == AlignTop)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignTop);
    else if((alignment & 0xf) == AlignVCenter)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignCenter);
    else if ((alignment & 0xf) == AlignBottom)
        buff.SetAlignment(TextBuffer::VerticalAlignment::AlignBottom);

    if ((alignment & 0xf0) == AlignLeft)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignLeft);
    else if ((alignment & 0xf0) == AlignHCenter)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignCenter);
    else if ((alignment & 0xf0) == AlignRight)
        buff.SetAlignment(TextBuffer::HorizontalAlignment::AlignRight);

    const bool underline = properties & TextProp::Underline;
    const bool blinking  = properties & TextProp::Blinking;

    // Add blob of text in the buffer.
    TextBuffer::Text text_and_style;
    text_and_style.text = text;
    text_and_style.font = font;
    text_and_style.fontsize = font_size_px;
    text_and_style.underline = underline;
    text_and_style.lineheight = line_height;
    buff.AddText(text_and_style);

    // Setup material to shade the text.
    static auto klass = std::make_shared<gfx::TextureMap2DClass>();
    klass->SetSurfaceType(MaterialClass::SurfaceType::Transparent);
    klass->SetBaseColor(color);
    klass->SetTexture(CreateTextureFromText(buff));
    klass->EnableGC(true);

    // if the text is set to be blinking do a sharp cut off
    // and when we have the "off" interval then simply don't
    // render the text.
    if (blinking)
    {
        const auto fps = 1.5;
        const auto full_period = 2.0 / fps;
        const auto half_period = full_period * 0.5;
        const auto time = fmodf(base::GetRuntimeSec(), full_period);
        if (time >= half_period)
            return;
    }

    Transform t;
    t.Resize(rect);
    t.MoveTo(rect);
    painter.Draw(Rectangle(), t, Material(klass));
}

void FillRect(Painter& painter, const FRect& rect, const Color4f& color)
{
    FillRect(painter, rect, MakeMaterial(color));
}

void FillRect(Painter& painter, const FRect& rect, const Material& material)
{
    FillShape(painter, rect, Rectangle(), material);
}

void FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Color4f& color)
{
    const float alpha = color.Alpha();
    FillShape(painter, rect, shape, MakeMaterial(color));
}
void FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Material& material)
{
    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform trans;
    trans.Resize(width, height);
    trans.Translate(x, y);
    painter.Draw(shape, trans, material);
}

void DrawRectOutline(Painter& painter, const FRect& rect, const Color4f& color, float line_width)
{
    DrawRectOutline(painter, rect, MakeMaterial(color));
}

void DrawRectOutline(Painter& painter, const FRect& rect, const Material& material, float line_width)
{
    DrawShapeOutline(painter, rect, gfx::Rectangle(), material, line_width);
}

void DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Color4f& color, float line_width)
{
    DrawShapeOutline(painter, rect, shape, MakeMaterial(color), line_width);
}
void DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Material& material, float line_width)
{
    // todo: this algorithm produces crappy results with diagonal lines
    // for example when drawing right angled triangle even with line widths > 1.0f
    // the results aren't looking that great.

    const auto width  = rect.GetWidth();
    const auto height = rect.GetHeight();
    const auto x = rect.GetX();
    const auto y = rect.GetY();

    Transform outline_transform;
    outline_transform.Resize(width, height);
    outline_transform.Translate(x, y);

    Transform mask_transform;
    const auto mask_width  = width - 2 * line_width;
    const auto mask_height = height - 2 * line_width;
    mask_transform.Resize(mask_width, mask_height);
    mask_transform.Translate(x + line_width, y + line_width);
    painter.Draw(shape, outline_transform, shape, mask_transform, material);
}

void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Color4f& color, float line_width)
{
    DrawLine(painter, a, b, MakeMaterial(color), line_width);
}

void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Material& material, float line_width)
{
    // The line shape defines a horizontal line so in order to
    // support lines with arbitrary directions we need to figure
    // out which way to rotate the line shape in order to have a matching
    // line (slope) and also how to scale the shape
    const auto& p = b - a;
    const auto x = p.GetX();
    const auto y = p.GetY();
    // pythagorean distance between the points is the length of the
    // line, used for horizontal scaling of the shape (along the X axis)
    const auto length = std::sqrt(x*x + y*y);
    const auto cosine = std::acos(x / length);
    // acos gives the principal angle [0, Pi], need to see if the
    // points would require a negative rotation.
    const auto angle  = a.GetY() > b.GetY() ? -cosine : cosine;

    Transform trans;
    trans.Scale(length, line_width);
    // offset by half the line width so that the vertical center of the
    // line aligns with the point. important when using line widths > 1.0f
    trans.Translate(0, -0.5*line_width);
    trans.Rotate(angle);
    trans.Translate(a);

    // Draw the shape (line)
    painter.Draw(Line(line_width), trans, material);
}

} // namespace

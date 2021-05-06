// Copyright (c) 2010-2019 Sami Väisänen, Ensisoft
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

#include <string>

#include "graphics/types.h"

// This is the simple drawing API that provides auxiliary drawing
// functionality mostly useful for drawing stuff outside the actual
// game content, for example in the editor application for drawing
// selection boxes etc.
//
// The drawing always takes place enclosed inside the rectangle which
// defines the bounding box for the shape/drawing operation. Shapes
// may or may not fill this rectangle completely.
//
// The coordinate of the bounding rect is relative to the origin of
// of the painter's logical view setting (see Painter::SetView)
// which means that the dimensions of the box may or may not equal
// pixels depending on what is the ratio of device viewport (pixels)
// to painters logical viewport size. The same mapping applies also
// to the position of the rect which is relative to the painter's
// view origin which maybe not be the same as the window (rendering
// surface origin) unless the painter has been configured so.
//
// In summary if you want to render to specific coordinates in the window
// and use pixels as the sizes for the shapes check the following:
//
// 1. Painter's device viewport is what you'd expect.
//    For 1:1 drawing to the window you'd probably want 0,0 as the
//    origin of the device viewport (window's top left corner) and
//    the size and width of the viewport should match the window's
//    *client* (i.e. the renderable surface) size. (Be aware of the
//    differences between *window size* and (renderable) surface size
//    aka as "client size".
//
// 2. Your pixel ratio in painter is 1:1, meaning 1 painter / game unit
//    maps to 1 pixel. This is adjusted through a call to Painter::SetView
//    You probably want to use Painter::SetTopLeftView.

namespace gfx
{

class Painter;
class Transform;
class Material;
class Drawable;
class Color4f;

// Text alignment inside a rect
enum TextAlign {
    // Vertical text alignment
    AlignTop     = 0x1,
    AlignVCenter = 0x2,
    AlignBottom  = 0x4,
    // Horizontal text alignment
    AlignLeft    = 0x10,
    AlignHCenter = 0x20,
    AlignRight   = 0x40
};

enum TextProp {
    Underline = 0x1,
    Blinking  = 0x2
};

// Draw text inside the given rectangle.
void DrawTextRect(Painter& painter,
    const std::string& text,
    const std::string& font,
    unsigned font_size_px,
    const FRect& rect,
    const Color4f& color,
    unsigned alignment = TextAlign::AlignVCenter | TextAlign::AlignHCenter,
    unsigned properties = 0x0,
    float line_height = 1.0f);

// Draw a rectangle filled with the desired color or material.
void FillRect(Painter& painter, const FRect& rect, const Color4f& color);
void FillRect(Painter& painter, const FRect& rect, const Material& material);

// Fill a shape within the specified rectangle with the desired color or material.
void FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Color4f& color);
void FillShape(Painter& painter, const FRect& rect, const Drawable& shape, const Material& material);

// Draw the outline of a rectangle. the rectangle is defined in pixels
// and positioned relative to the top left corer of the render target/surface.
// If rotation is non-zero the rect is first rotated *then* translated.
void DrawRectOutline(Painter& painter, const FRect& rect, const Color4f& color, float line_width = 1.0f);
void DrawRectOutline(Painter& painter, const FRect& rect, const Material& material, float line_width = 1.0f);

void DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Color4f& color, float line_width = 1.0f);
void DrawShapeOutline(Painter& painter, const FRect& rect, const Drawable& shape,
                      const Material& material, float line_width = 1.0f);

// Draw a line from the center of point A to the center of point B
// using the given line width (if possible) and with the given color.
// Points A and B are relative to the top left corner of the rendering
// target (e.g the window surface).
void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Color4f& color, float line_width = 1.0f);
// Like above but instead use a material for rasterizing the line fragments.
void DrawLine(Painter& painter, const FPoint& a, const FPoint& b, const Material& material, float line_width = 1.0f);

} // namespace

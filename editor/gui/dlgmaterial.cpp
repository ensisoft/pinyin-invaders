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

#define LOGTAG "gui"

#include "config.h"

#include "warnpush.h"

#include "warnpop.h"

#include "editor/app/workspace.h"
#include "editor/app/eventlog.h"
#include "editor/gui/dlgmaterial.h"
#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/drawing.h"

namespace {
    constexpr unsigned BoxSize = 100;
    constexpr unsigned BoxMargin = 20;
} // namespace

namespace gui
{

DlgMaterial::DlgMaterial(QWidget* parent, const app::Workspace* workspace, const QString& material)
  : QDialog(parent)
  , mWorkspace(workspace)
  , mSelectedMaterialId(material)
{
    mUI.setupUi(this);

    setMouseTracking(true);
    // do the graphics dispose in finished handler which is triggered
    // regardless whether we do accept/reject or the user clicks the X
    // or presses Esc.
    connect(this, &QDialog::finished, mUI.widget, &GfxWidget::dispose);
    // render on timer
    connect(&mTimer, &QTimer::timeout, mUI.widget, &GfxWidget::triggerPaint);

    mUI.widget->onPaintScene = std::bind(&DlgMaterial::PaintScene,
        this, std::placeholders::_1, std::placeholders::_2);
    mUI.widget->onInitScene = [&](unsigned, unsigned) {
        mTimer.setInterval(1000.0/60.0);
        mTimer.start();
    };
    mUI.widget->onKeyPress = std::bind(&DlgMaterial::KeyPress, this, std::placeholders::_1);
    mUI.widget->onMousePress = std::bind(&DlgMaterial::MousePress, this, std::placeholders::_1);
    mUI.widget->onMouseWheel = std::bind(&DlgMaterial::MouseWheel, this, std::placeholders::_1);
    mUI.widget->onMouseDoubleClick = std::bind(&DlgMaterial::MouseDoubleClick, this, std::placeholders::_1);
}

void DlgMaterial::on_btnAccept_clicked()
{
    accept();
}
void DlgMaterial::on_btnCancel_clicked()
{
    reject();
}

void DlgMaterial::on_vScroll_valueChanged()
{
    mScrollOffsetRow = mUI.vScroll->value();
}

void DlgMaterial::PaintScene(gfx::Painter& painter, double secs)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();
    painter.SetViewport(0, 0, width, height);

    const auto num_visible_cols = width / (BoxSize + BoxMargin);
    const auto num_visible_rows = height / (BoxSize + BoxMargin);
    const auto xoffset  = (width - ((BoxSize + BoxMargin) * num_visible_cols)) / 2;
    const auto yoffset  = -mScrollOffsetRow * (BoxSize + BoxMargin);
    unsigned index = 0;

    mMaterialIds.clear();

    for (size_t i=0; i<mWorkspace->GetNumResources(); ++i)
    {
        const auto& resource = mWorkspace->GetResource(i);
        if (!resource.IsMaterial())
            continue;
        mMaterialIds.push_back(resource.GetId());
        auto klass = app::ResourceCast<gfx::MaterialClass>(resource).GetSharedResource();

        const auto col = index % num_visible_cols;
        const auto row = index / num_visible_cols;
        const auto xpos = xoffset + col * (BoxSize + BoxMargin);
        const auto ypos = yoffset + row * (BoxSize + BoxMargin);

        gfx::FRect  rect;
        rect.Resize(BoxSize, BoxSize);
        rect.Move(xpos, ypos);
        rect.Translate(BoxMargin*0.5f, BoxMargin*0.5f);
        gfx::FillRect(painter, rect, klass);

        if (resource.GetId() == mSelectedMaterialId)
        {
            gfx::DrawRectOutline(painter, rect, gfx::Color::Green, 2.0f);
        }
        ++index;
    }

    const auto num_total_rows = index / num_visible_cols + 1;
    if (num_total_rows > num_visible_rows)
    {
        const auto num_scroll_steps = num_total_rows - num_visible_rows;
        QSignalBlocker blocker(mUI.vScroll);
        mUI.vScroll->setVisible(true);
        mUI.vScroll->setMaximum(num_scroll_steps);
        if (num_visible_rows != mNumVisibleRows)
        {
            mUI.vScroll->setValue(0);
            mNumVisibleRows = num_visible_rows;
        }
    }
    else
    {
        mUI.vScroll->setVisible(false);
    }
}

void DlgMaterial::MousePress(QMouseEvent* mickey)
{
    const auto width  = mUI.widget->width();
    const auto height = mUI.widget->height();

    const auto num_cols = width / (BoxSize + BoxMargin);
    const auto xoffset  = (width - ((BoxSize + BoxMargin) * num_cols)) / 2;
    const auto yoffset  = mScrollOffsetRow * (BoxSize + BoxMargin);

    const auto widget_xpos = mickey->pos().x();
    const auto widget_ypos = mickey->pos().y();
    const auto col = (widget_xpos - xoffset) / (BoxSize + BoxMargin);
    const auto row = (widget_ypos + yoffset) / (BoxSize + BoxMargin);
    const auto index = row * num_cols + col;

    if (index >= mMaterialIds.size())
        return;
    mSelectedMaterialId = mMaterialIds[index];
}

void DlgMaterial::MouseDoubleClick(QMouseEvent* mickey)
{
    MousePress(mickey);
    accept();
}

void DlgMaterial::MouseWheel(QWheelEvent* wheel)
{
    const QPoint& num_degrees = wheel->angleDelta() / 8;
    const QPoint& num_steps = num_degrees / 15;
    // only consider the wheel scroll steps on the vertical
    // axis for zooming.
    // if steps are positive the wheel is scrolled away from the user
    // and if steps are negative the wheel is scrolled towards the user.
    const int num_zoom_steps = num_steps.y();

    unsigned max = mUI.vScroll->maximum();

    for (int i=0; i<std::abs(num_zoom_steps); ++i)
    {
        if (num_zoom_steps > 0)
            mScrollOffsetRow = mScrollOffsetRow > 0 ? mScrollOffsetRow - 1 : 0;
        else if (num_zoom_steps < 0)
            mScrollOffsetRow = mScrollOffsetRow < max ? mScrollOffsetRow + 1 : mScrollOffsetRow;
    }

    QSignalBlocker blocker(mUI.vScroll);
    mUI.vScroll->setValue(mScrollOffsetRow);
}

bool DlgMaterial::KeyPress(QKeyEvent* key)
{
    if (key->key() == Qt::Key_Escape)
    {
        reject();
        return true;
    }
    return false;
}

} // namespace

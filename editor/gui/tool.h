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
#  include <QPoint>
#  include <QMouseEvent>
#  include <QString>
#  include <glm/glm.hpp>
#  include <glm/gtx/matrix_decompose.hpp>
#include "warnpop.h"

#include <memory>
#include <cmath>

#include "graphics/painter.h"
#include "graphics/material.h"
#include "graphics/drawable.h"
#include "graphics/transform.h"

namespace gui
{
    // Interface for transforming simple mouse actions
    // into actions that manipulate some state such as
    // animation/scene render tree.
    class MouseTool
    {
    public:
        virtual ~MouseTool() = default;
        // Render the visualization of the current tool and/or the action
        // being performed.
        virtual void Render(gfx::Painter& painter, gfx::Transform& view) const {}
        // Act on mouse move event.
        virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& view) = 0;
        // Act on mouse press event.
        virtual void MousePress(QMouseEvent* mickey, gfx::Transform& view) = 0;
        // Act on mouse release event. Typically this completes the tool i.e.
        // the use and application of this tool.
        // The return value indicates whether the application of tool resulted
        // in any state change. If any state was changed then true is returned.
        // If the tool application was cancelled then false is returned.
        virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& view) = 0;
    private:
    };

    // Move/translate the camera.
    template<typename CameraState>
    class MoveCameraTool : public MouseTool
    {
    public:
        explicit
        MoveCameraTool(CameraState& state) : mState(state)
        {}
        virtual void Render(gfx::Painter& painter, gfx::Transform&) const override
        {}
        virtual void MouseMove(QMouseEvent* mickey, gfx::Transform&) override
        {
            if (mEngaged)
            {
                const auto& pos = mickey->pos();
                const auto& delta = pos - mMousePos;
                const float x = delta.x();
                const float y = delta.y();
                mState.camera_offset_x += x;
                mState.camera_offset_y += y;
                //DEBUG("Camera offset %1, %2", mState.camera_offset_x, mState.camera_offset_y);
                mMousePos = pos;
            }
        }
        virtual void MousePress(QMouseEvent* mickey, gfx::Transform&) override
        {
            const auto button = mickey->button();
            if (button == Qt::LeftButton)
            {
                mMousePos = mickey->pos();
                mEngaged = true;
            }
        }
        virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform&) override
        {
            const auto button = mickey->button();
            if (button == Qt::LeftButton)
            {
                mEngaged = false;
                return false;
            }
            return true;
        }
    private:
        CameraState& mState;
    private:
        QPoint mMousePos;
        bool mEngaged = false;
    };

    template<typename TreeModel, typename TreeNode>
    class MoveRenderTreeNodeTool : public MouseTool
    {
    public:
        MoveRenderTreeNodeTool(TreeModel& model, TreeNode* selected)
                : mModel(model)
                , mNode(selected)
        {}
        virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);

            const auto& tree      = mModel.GetRenderTree();
            const auto& tree_node = tree.FindNodeByValue(mNode);
            const auto* parent    = tree.FindParent(tree_node);
            // if the object we're moving has a parent we need to map the mouse movement
            // correctly taking into account that the hierarchy might include several rotations.
            // simplest thing to do is to map the mouse to the object's parent's coordinate space
            // and thus express/measure the object's translation delta relative to it's parent
            // (as it is in the hierarchy).
            // todo: this could be simplified if we expressed the view transformation in the render tree's
            // root node. then the below else branch should go away(?)
            if (parent && parent->GetValue())
            {
                const auto& mouse_pos_in_node = mModel.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
                const auto& mouse_delta = mouse_pos_in_node - mPreviousMousePos;

                glm::vec2 position = mNode->GetTranslation();
                position.x += mouse_delta.x;
                position.y += mouse_delta.y;
                mNode->SetTranslation(position);
                mPreviousMousePos = mouse_pos_in_node;
            }
            else
            {
                // object doesn't have a parent, movement can be expressed using the animations
                // coordinate space.
                const auto& mouse_delta = mouse_pos_in_view - glm::vec4(mPreviousMousePos, 0.0f, 0.0f);
                glm::vec2 position = mNode->GetTranslation();
                position.x += mouse_delta.x;
                position.y += mouse_delta.y;
                mNode->SetTranslation(position);
                mPreviousMousePos = glm::vec2(mouse_pos_in_view.x, mouse_pos_in_view.y);
            }
        }
        virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);

            // see the comments in MouseMove about the branched logic.
            const auto& tree      = mModel.GetRenderTree();
            const auto& tree_node = tree.FindNodeByValue(mNode);
            const auto* parent    = tree.FindParent(tree_node);
            if (parent && parent->GetValue())
            {
                mPreviousMousePos = mModel.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, parent->GetValue());
            }
            else
            {
                mPreviousMousePos = glm::vec2(mouse_pos_in_view.x, mouse_pos_in_view.y);
            }
        }
        virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            // nothing to be done here.
            return false;
        }
    private:
        TreeModel& mModel;
        TreeNode*  mNode = nullptr;
        // previous mouse position, for each mouse move we update the objects'
        // position by the delta between previous and current mouse pos.
        glm::vec2 mPreviousMousePos;
    };

    template<typename TreeModel, typename TreeNode>
    class ResizeRenderTreeNodeTool : public MouseTool
    {
    public:
        ResizeRenderTreeNodeTool(TreeModel& model, TreeNode* selected)
                : mModel(model)
                , mNode(selected)
        {}
        virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
            const auto& mouse_pos_in_node = mModel.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);
            const auto& mouse_delta = (mouse_pos_in_node - mPreviousMousePos);
            const bool maintain_aspect_ratio = mickey->modifiers() & Qt::ControlModifier;

            if (maintain_aspect_ratio)
            {
                const glm::vec2& size = mNode->GetSize();

                const auto aspect_ratio = size.x / size.y;
                const auto new_height = std::clamp(size.y + mouse_delta.y, 0.0f, size.y + mouse_delta.y);
                const auto new_width  = new_height * aspect_ratio;
                mNode->SetSize(glm::vec2(new_width, new_height));
            }
            else
            {
                glm::vec2 size = mNode->GetSize();
                // don't allow negative sizes.
                size.x = std::clamp(size.x + mouse_delta.x, 0.0f, size.x + mouse_delta.x);
                size.y = std::clamp(size.y + mouse_delta.y, 0.0f, size.y + mouse_delta.y);
                mNode->SetSize(size);
            }
            mPreviousMousePos = mouse_pos_in_node;
        }
        virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
            mPreviousMousePos = mModel.MapCoordsToNode(mouse_pos_in_view.x, mouse_pos_in_view.y, mNode);
        }
        virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            // nothing to be done here.
            return false;
        }
    private:
        TreeModel& mModel;
        TreeNode* mNode = nullptr;
        // previous mouse position, for each mouse move we update the objects'
        // position by the delta between previous and current mouse pos.
        glm::vec2 mPreviousMousePos;
    };

    template<typename TreeModel, typename TreeNode>
    class RotateRenderTreeNodeTool : public MouseTool
    {
    public:
        RotateRenderTreeNodeTool(TreeModel& model, TreeNode* selected)
                : mModel(model)
                , mNode(selected)
        {}
        virtual void MouseMove(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            const auto& mouse_pos_in_view = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
            const auto& node_size = mNode->GetSize();
            const auto& node_center_in_view = glm::vec4(mModel.MapCoordsFromNode(node_size.x*0.5f, node_size.y*0.5, mNode),1.0f, 1.0f);
            // compute the delta between the current mouse position angle and the previous mouse position angle
            // wrt the node's center point. Then and add the angle delta increment to the node's rotation angle.
            const auto previous_angle = GetAngleRadians(mPreviousMousePos - node_center_in_view);
            const auto current_angle  = GetAngleRadians(mouse_pos_in_view - node_center_in_view);
            const auto angle_delta = current_angle - previous_angle;

            double angle = mNode->GetRotation();
            angle += angle_delta;
            // keep it in the -180 - 180 degrees [-Pi, Pi] range.
            angle = math::wrap(-math::Pi, math::Pi, angle);
            mNode->SetRotation(angle);
            mPreviousMousePos = mouse_pos_in_view;
        }
        virtual void MousePress(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            const auto& mouse_pos = mickey->pos();
            const auto& widget_to_view = glm::inverse(trans.GetAsMatrix());
            mPreviousMousePos = widget_to_view * glm::vec4(mouse_pos.x(), mouse_pos.y(), 1.0f, 1.0f);
        }
        virtual bool MouseRelease(QMouseEvent* mickey, gfx::Transform& trans) override
        {
            return false;
        }
    private:
        float GetAngleRadians(const glm::vec4& p) const
        {
            const auto hypotenuse = std::sqrt(p.x*p.x + p.y*p.y);
            // acos returns principal angle range which is [0, Pi] radians
            // but we want to map to a range of [0, 2*Pi] i.e. full circle.
            // therefore we check the Y position.
            const auto principal_angle = std::acos(p.x / hypotenuse);
            if (p.y < 0.0f)
                return math::Pi * 2.0 - principal_angle;
            return principal_angle;
        }
    private:
        TreeModel& mModel;
        TreeNode* mNode = nullptr;
        // previous mouse position, for each mouse move we update the object's
        // position by the delta between previous and current mouse pos.
        glm::vec4 mPreviousMousePos;
    };


} // namespace
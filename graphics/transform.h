// Copyright (c) 2010-2018 Sami Väisänen, Ensisoft
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

namespace gfx
{
    class Transform
    {
    public:
        Transform() = default;

        Transform(float sx, float sy, float x, float y)
          : mScaleX(sx)
          , mScaleY(sy)
          , mPosX(x)
          , mPosY(y)
        {}

        void MoveTo(float x, float y)
        {
            mPosX = x;
            mPosY = y;
        }

        void Resize(float sx, float sy)
        {
            mScaleX = sx;
            mScaleY = sy;
        }

        template<typename T>
        void MoveTo(const T& t)
        {
            mPosX = t.x();
            mPosY = t.y();
        }

        template<typename T>
        void Resize(const T& t)
        {
            mScaleX = t.width();
            mScaleY = t.height();
        }

        // around the depth axis, positive angle (expressed in degrees)
        // is clockwise rotation.
        void Rotate(float degrees)
        { mRotation = degrees; }

        float GetWidth() const
        { return mScaleX; }

        float GetHeight() const
        { return mScaleY; }

        float GetXPosition() const
        { return mPosX; }

        float GetYPosition() const
        { return mPosY; }

        float GetRotation() const
        { return mRotation; }

    private:
        float mScaleX = 1.0f;
        float mScaleY = 1.0f;
        float mPosX = 0.0f;
        float mPosY = 0.0f;
        float mRotation = 0.0f;
    };

} // namespace

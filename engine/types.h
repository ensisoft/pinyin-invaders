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
#  include <glm/glm.hpp>
#  include <nlohmann/json_fwd.hpp>
#include "warnpop.h"

#include <cmath>
#include <variant>
#include <string>
#include <optional>

#include "base/assert.h"
// todo: refactor the include away.
#include "graphics/types.h"

namespace game
{
    // provide type aliases for now for these types so that we can
    // use them as if they weren't in graphics where they shouldn't
    // be for most of the use in engine code. (i.e. not related to
    // graphics in any way)
    // todo: eventually should refactor them out of graphics/ into base/
    using FRect = gfx::FRect;
    using IRect = gfx::IRect;
    using IPoint = gfx::IPoint;
    using FPoint = gfx::FPoint;
    using FSize  = gfx::FSize;
    using ISize  = gfx::ISize;

    // Value supporting "arbitrary" values for scripting environments
    // such as Lua.
    class ScriptVar
    {
    public:
        static constexpr bool ReadOnly = true;
        static constexpr bool ReadWrite = false;

        // The types of values supported by the ScriptVar.
        enum class Type {
            String,
            Integer,
            Float,
            Vec2,
            Boolean
        };
        template<typename T>
        ScriptVar(std::string name, T value, bool read_only = true)
          : mName(std::move(name))
          , mData(std::move(value))
          , mReadOnly(read_only)
        {}

        // Get whether the variable is considered read-only/constant
        // in the scripting environment.
        bool IsReadOnly() const
        { return mReadOnly; }
        // Get the type of the variable.
        Type GetType() const;
        // Get the script variable name.
        std::string GetName() const
        { return mName; }
        // Get the actual value. The ScriptVar *must* hold that
        // type internally for the data item.
        template<typename T>
        T GetValue() const
        {
            ASSERT(std::holds_alternative<T>(mData));
            return std::get<T>(mData);
        }
        // Set a new value in the script var. The value must
        // have the same type as previously (i.e. always match the
        // type of the encapsulated value inside ScriptVar) and
        // additionally not be read only.
        // marked const to allow the value that is held to change while
        // retaining the semantical constness of being a C++ ScriptVar object.
        // The value however can change since that "constness" is expressed
        // with the boolean read-only flag.
        template<typename T>
        void SetValue(T value) const
        {
            ASSERT(std::holds_alternative<T>(mData));
            ASSERT(mReadOnly == false);
            mData = std::move(value);
        }

        template<typename T>
        bool HasType() const
        { return std::holds_alternative<T>(mData); }

        // Get the hash value of the current parameters.
        size_t GetHash() const;

        // Serialize into JSON.
        nlohmann::json ToJson() const;

        static std::optional<ScriptVar> FromJson(const nlohmann::json& json);
    private:
        ScriptVar() = default;
        // name of the variable in the script.
        std::string mName;
        // the actual data.
        mutable std::variant<bool, float, int, std::string, glm::vec2> mData;
        // whether the variable is a read-only / constant in the
        // scripting environment. read only variables can be
        // shared by multiple object instances thus leading to
        // reduced memory consumption. (hence the default)
        bool mReadOnly = true;
    };

    // Box represents a rectangular object which (unlike gfx::FRect)
    // also maintains orientation.
    class FBox
    {
    public:
        // Create a new unit box by default.
        FBox(float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
        FBox(const glm::mat4& mat, float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, 0.0f)));
            mTopRight = ToVec2(mat * ToVec4(glm::vec2(w, 0.0f)));
            mBotLeft  = ToVec2(mat * ToVec4(glm::vec2(0.0f, h)));
            mBotRight = ToVec2(mat * ToVec4(glm::vec2(w, h)));
        }
        void Transform(const glm::mat4& mat)
        {
            mTopLeft  = ToVec2(mat * ToVec4(mTopLeft));
            mTopRight = ToVec2(mat * ToVec4(mTopRight));
            mBotLeft  = ToVec2(mat * ToVec4(mBotLeft));
            mBotRight = ToVec2(mat * ToVec4(mBotRight));
        }
        float GetWidth() const
        { return glm::length(mTopRight - mTopLeft); }
        float GetHeight() const
        { return glm::length(mBotLeft - mTopLeft); }
        float GetRotation() const
        {
            const auto dir = glm::normalize(mTopRight - mTopLeft);
            const auto cosine = glm::dot(glm::vec2(1.0f, 0.0f), dir);
            if (dir.y < 0.0f)
                return -std::acos(cosine);
            return std::acos(cosine);
        }
        glm::vec2 GetTopLeft() const
        { return mTopLeft; }
        glm::vec2 GetTopRight() const
        { return mTopRight; }
        glm::vec2 GetBotLeft() const
        { return mBotLeft; }
        glm::vec2 GetBotRight() const
        { return mBotRight; }
        glm::vec2 GetPosition() const
        {
            const auto diagonal = mBotRight - mTopLeft;
            return mTopLeft + diagonal * 0.5f;
        }
        glm::vec2 GetSize() const
        { return glm::vec2(GetWidth(), GetHeight()); }
        void Reset(float w = 1.0f, float h = 1.0f)
        {
            mTopLeft  = glm::vec2(0.0f, 0.0f);
            mTopRight = glm::vec2(w, 0.0f);
            mBotLeft  = glm::vec2(0.0f, h);
            mBotRight = glm::vec2(w, h);
        }
    private:
        static glm::vec4 ToVec4(const glm::vec2& vec)
        { return glm::vec4(vec.x, vec.y, 1.0f, 1.0f); }
        static glm::vec2 ToVec2(const glm::vec4& vec)
        { return glm::vec2(vec.x, vec.y); }
    private:
        // store the box as 4 2d points each
        // representing one corner of the box.
        // there are alternative ways too, such as
        // position + dim vectors and rotation
        // but this representation is quite simple.
        glm::vec2 mTopLeft;
        glm::vec2 mTopRight;
        glm::vec2 mBotLeft;
        glm::vec2 mBotRight;
    };

    inline FBox TransformBox(const FBox& box, const glm::mat4& mat)
    {
        FBox ret(box);
        ret.Transform(mat);
        return ret;
    }

} // namespace


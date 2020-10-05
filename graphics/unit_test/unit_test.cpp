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

#include "config.h"

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "graphics/types.h"
#include "graphics/color4f.h"
#include "graphics/drawable.h"

bool operator==(const gfx::Color4f& lhs, const gfx::Color4f& rhs)
{
    return real::equals(lhs.Red(), rhs.Red()) &&
           real::equals(lhs.Green(), rhs.Green()) &&
           real::equals(lhs.Blue(), rhs.Blue()) &&
           real::equals(lhs.Alpha(), rhs.Alpha());
}

bool operator==(const gfx::Rect<float>& lhs,
                const gfx::Rect<float>& rhs)
{
    return real::equals(lhs.GetX(), rhs.GetX()) &&
            real::equals(lhs.GetY(), rhs.GetY()) &&
            real::equals(lhs.GetWidth(), rhs.GetWidth()) &&
            real::equals(lhs.GetHeight(), rhs.GetHeight());
}

template<typename T>
bool operator==(const gfx::Rect<T>& lhs,
                const gfx::Rect<T>& rhs)
{
    return lhs.GetX() == rhs.GetX() &&
        lhs.GetY() == rhs.GetY() &&
        lhs.GetWidth() == rhs.GetWidth() &&
        lhs.GetHeight() == rhs.GetHeight();
}

bool operator==(const gfx::Vertex& lhs, const gfx::Vertex& rhs)
{
    return real::equals(lhs.aPosition.x, rhs.aPosition.x) &&
           real::equals(lhs.aPosition.y, rhs.aPosition.y) &&
           real::equals(lhs.aTexCoord.x, rhs.aTexCoord.x) &&
           real::equals(lhs.aTexCoord.y, rhs.aTexCoord.y);
}

template<typename T>
void unit_test_rect_intersect()
{
    using R = gfx::Rect<T>;

    struct TestCase {
        R lhs;
        R rhs;
        R expected_result;
    } cases[] = {
        // empty rect no overlap
        {
            R(0, 0, 0, 0),
            R(0, 0, 1, 1),
            R()
        },
        // empty rect, no overlap
        {
            R(0, 0, 1, 1),
            R(0, 0, 0, 0),
            R()
        },

        // no overlap on x axis
        { R(0, 0, 10, 10),
          R(10, 0, 10, 10),
          R()
        },

        // no overlap on x axis
        {
            R(0, 0, 10, 10),
            R(-10, 0, 10, 10),
            R()
        },

        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, 10, 10, 10),
            R()
        },

        // no overlap on y axis
        {
            R(0, 0, 10, 10),
            R(0, -10, 10, 10),
            R()
        },

        // overlaps itself
        {
            R(0, 0, 10, 10),
            R(0, 0, 10, 10),
            R(0, 0, 10, 10)
        },

        // sub rectangle within one overlaps
        {
            R(0, 0, 10, 10),
            R(2, 2, 5, 5),
            R(2, 2, 5, 5)
        },

        // overlap in bottom right corner
        {
            R(0, 0, 10, 10),
            R(5, 5, 10, 10),
            R(5, 5, 5, 5)
        },

        // overlap in top left corner
        {
            R(0, 0, 10, 10),
            R(-5, -5, 10, 10),
            R(0, 0, 5, 5)
        }

    };

    for (const auto& test : cases)
    {
        const auto& ret = Intersect(test.lhs, test.rhs);
        TEST_REQUIRE(ret == test.expected_result);
    }
}

template<typename T>
void unit_test_rect_union()
{
    using R = gfx::Rect<T>;

    struct TestCase {
        R lhs;
        R rhs;
        R expected_result;
    } cases[] = {
        // empty rectangle
        {
            R(0, 0, 0, 0),
            R(0, 0, 10, 10),
            R(0, 0, 10, 10)
        },
        // empty rectangle
        {
            R(0, 0, 10, 10),
            R(0, 0, 0, 0),
            R(0, 0, 10, 10)
        },

        // disjoint rectangles
        {
            R(0, 0, 5, 5),
            R(5, 5, 5, 5),
            R(0, 0, 10, 10)
        },

        // disjoint rectangles, negative values.
        {
            R(-5, -5, 5, 5),
            R(-10, -10, 5, 5),
            R(-10, -10, 10, 10)
        },

        // overlapping rectangles
        {
            R(20, 20, 10, 10),
            R(25, 25, 5, 5),
            R(20, 20, 10, 10)
        }
    };

    for (const auto& test : cases)
    {
        const auto& ret = Union(test.lhs, test.rhs);
        TEST_REQUIRE(ret == test.expected_result);
    }
}

template<typename T>
void unit_test_rect_serialize()
{
    const T test_values[] = {
        T(0), T(1.5), T(100), T(-40), T(125.0)
    };
    for (const auto& val : test_values)
    {
        const gfx::Rect<T> src(val, val, val, val);
        const auto& value = gfx::Rect<T>::FromJson(src.ToJson());
        TEST_REQUIRE(value.has_value());
        TEST_REQUIRE(value.value() == src);
    }
}

void unit_test_color_serialize()
{
    const float test_values[] = {
        0.0f, 0.2f, 0.5f, 1.0f
    };
    for (const float val : test_values)
    {
        {
            const gfx::Color4f src(val, val, val, val);
            const auto& value = gfx::Color4f::FromJson(src.ToJson());
            TEST_REQUIRE(value.has_value());
            TEST_REQUIRE(value.value() == src);
        }
        {
            bool success = true;
            const auto& json = nlohmann::json::parse(
                R"({"r":0.0, "g":"basa", "b":0.0, "a":0.0})");

            const auto& value = gfx::Color4f::FromJson(json);
            TEST_REQUIRE(value.has_value() == false);
        }
    }
}

void unit_test_polygon_serialize()
{
    std::vector<gfx::Vertex> verts;
    gfx::Vertex v0;
    v0.aPosition.x = 1.0f;
    v0.aPosition.y = 2.0f;
    v0.aTexCoord.x = -1.0f;
    v0.aTexCoord.y = -0.5f;
    verts.push_back(v0);

    gfx::Polygon poly;
    gfx::Polygon::DrawCommand cmd;
    cmd.type = gfx::Polygon::DrawType::TriangleFan;
    cmd.offset = 1243;
    cmd.count = 555;
    poly.AddDrawCommand(std::move(verts), cmd);

    const auto& data = poly.ToJson();

    poly.Clear();
    TEST_REQUIRE(poly.GetNumVertices() == 0);
    TEST_REQUIRE(poly.GetNumDrawCommands() == 0);

    const auto& result = gfx::Polygon::FromJson(data);
    TEST_REQUIRE(result.has_value());

    poly = std::move(result.value());
    TEST_REQUIRE(poly.GetNumVertices() == 1);
    TEST_REQUIRE(poly.GetNumDrawCommands() == 1);
    TEST_REQUIRE(poly.GetVertex(0) == v0);
    TEST_REQUIRE(poly.GetDrawCommand(0).type == gfx::Polygon::DrawType::TriangleFan);
    TEST_REQUIRE(poly.GetDrawCommand(0).offset == 1243);
    TEST_REQUIRE(poly.GetDrawCommand(0).count == 555);
}

int test_main(int argc, char* argv[])
{
    unit_test_rect_intersect<float>();
    unit_test_rect_intersect<int>();
    unit_test_rect_union<float>();
    unit_test_rect_union<int>();
    unit_test_rect_serialize<int>();
    unit_test_rect_serialize<float>();
    unit_test_color_serialize();
    unit_test_polygon_serialize();
    return 0;
}

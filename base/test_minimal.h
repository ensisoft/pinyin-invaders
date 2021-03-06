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

#pragma once

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#if defined(_MSC_VER)
#  include <Windows.h> // for DebugBreak
#endif

namespace test {

static
void blurb_failure(const char* expression,
    const char* file,
    const char* function,
    int line, bool fatality)
{
    std::printf("\n%s(%d): %s failed in function: '%s'\n", file, line, expression, function);
    if (fatality)
    {
#if defined(_MSC_VER)
        DebugBreak();
#elif defined(__GNUG__)
        __builtin_trap();
#endif
        std::abort();
    }

}

} // test

#define TEST_CHECK(expr) \
    (expr) \
    ? ((void)0) \
    : (test::blurb_failure(#expr, __FILE__, __FUNCTION__, __LINE__, false))

#define TEST_REQUIRE(expr) \
    (expr) \
    ? ((void)0) \
    : (test::blurb_failure(#expr, __FILE__, __FUNCTION__, __LINE__, true))


#define TEST_MESSAGE(msg, ...) \
    std::printf("%s (%d): '" msg "'\n", __FUNCTION__, __LINE__, ## __VA_ARGS__); \
    std::fflush(stdout)

#define TEST_EXCEPTION(expr) \
    try { \
        expr; \
        TEST_REQUIRE(!"Exception was expected"); \
    } \
    catch (const std::exception& e) \
    {}


int test_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    try
    {
        test_main(argc, argv);
        std::printf("\nSuccess!\n");
    }
    catch (const std::exception & e)
    {
        std::printf("Tests didn't run to completion because an exception occurred!\n\n");
        std::printf("%s\n", e.what());
        return 1;
    }
    return 0;
}

#define TEST_BARK() \
    std::printf("%s", __FUNCTION__); \
    std::fflush(stdout)


#define TEST_CUSTOM
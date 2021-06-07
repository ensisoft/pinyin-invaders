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

#include <chrono>
#include <locale>
#include <codecvt>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <cstring>

#include "base/utility.h"

#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4996) // deprecated use of wstring_convert
#endif

namespace base
{

double GetRuntimeSec()
{
    using steady_clock = std::chrono::steady_clock;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    const auto gone = now - start;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(gone);
    return ms.count() / 1000.0;
}

std::string RandomString(size_t len)
{
    static const char* alphabet =
            "ABCDEFGHIJKLMNOPQRSTUWXYZ"
            "abdefghijlkmnopqrstuvwxyz"
            "1234567890";
    static unsigned max_len = std::strlen(alphabet) - 1;
    static std::random_device rd;
    std::uniform_int_distribution<unsigned> dist(0, max_len);

    std::string ret;
    for (size_t i=0; i<len; ++i)
    {
        const auto letter = dist(rd);
        ret.push_back(alphabet[letter]);
    }
    return ret;
}

std::string ToUtf8(const std::wstring& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}


std::wstring FromUtf8(const std::string& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::wstring converted_str = converter.from_bytes(str);
    return converted_str;
}

std::wstring ToUpper(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towupper(c));
    return ret;
}


std::wstring ToLower(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towlower(c));
    return ret;
}

// convert std::string to wide string representation
// without any regard to encoding.
std::wstring Widen(const std::string& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(c);
    return ret;
}

bool FileExists(const std::string& filename)
{
#if defined(WINDOWS_OS)
    const std::filesystem::path p(base::FromUtf8(filename));
return std::filesystem::exists(p);
#elif defined(POSIX_OS)
    const std::filesystem::path p(filename);
    return std::filesystem::exists(p);
#endif
}

std::string JoinPath(const std::string& a, const std::string& b)
{
    const std::filesystem::path pa(a);
    const std::filesystem::path pb(b);
    return (pa / pb).generic_u8string();
}

std::ifstream OpenBinaryInputStream(const std::string& filename)
{
#if defined(WINDOWS_OS)
    std::ifstream in(base::FromUtf8(filename), std::ios::in | std::ios::binary);
#elif defined(POSIX_OS)
    std::ifstream in(filename, std::ios::in | std::ios::binary);
#else
#  error unimplemented function
#endif
    return in;
}

bool OverwriteTextFile(const std::string& file, const std::string& text)
{
#if defined(WINDOWS_OS)
    std::ofstream out(base::FromUtf8(file), std::ios::out);
#elif defined(POSIX_OS)
    std::ofstream out(file, std::ios::out | std::ios::trunc);
#else
#  error unimplemented function.
#endif
    if (!out.is_open())
        return false;
    out << text;
    if (out.bad() || out.fail())
        return false;
    return true;
}

} // namespace

#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif
// Copyright (c) 2016 Sami Väisänen, Ensisoft
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
#  include <neargye/magic_enum.hpp>
#include "warnpop.h"

#include <functional> // for hash
#include <chrono>
#include <memory> // for unique_ptr
#include <string>
#include <locale>
#include <codecvt>
#include <cwctype>

#if defined(__MSVC__)
#  pragma warning(push)
#  pragma warning(disable: 4996) // deprecated use of wstring_convert
#endif

namespace base
{

template<typename S, typename T> inline
S hash_combine(S seed, T value)
{
    const auto hash = std::hash<T>()(value);
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}


inline double GetRuntimeSec()
{
    using steady_clock = std::chrono::steady_clock;
    static const auto start = steady_clock::now();
    const auto now = steady_clock::now();
    const auto gone = now - start;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(gone);
    return ms.count() / 1000.0;
}

template<typename T, typename Deleter>
std::unique_ptr<T, Deleter> MakeUniqueHandle(T* ptr, Deleter del)
{
    return std::unique_ptr<T, Deleter>(ptr, del);
}

inline
std::string ToUtf8(const std::wstring& str)
{
    // this way of converting is depcreated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::string converted_str = converter.to_bytes(str);
    return converted_str;
}

inline
std::wstring FromUtf8(const std::string& str)
{
    // this way of converting is deprecated since c++17 but
    // this works good enough for now so we'll go with it.
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    std::wstring converted_str = converter.from_bytes(str);
    return converted_str;
}

inline
std::wstring ToUpper(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towupper(c));
    return ret;
}

inline
std::wstring ToLower(const std::wstring& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(std::towlower(c));
    return ret;
}

// convert std::string to wide string representation
// without any regard to encoding.
inline
std::wstring Widen(const std::string& str)
{
    std::wstring ret;
    for (auto c : str)
        ret.push_back(c);
    return ret;
}

// works with nlohmann::json
// using a template here simply to avoid having to specify a type
// that would require dependency to a library in this header.
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, float* out)
{
    if (!object.contains(name) || !object[name].is_number_float())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, int* out)
{
    if (!object.contains(name) || !object[name].is_number_integer())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, unsigned* out)
{
    if (!object.contains(name) || !object[name].is_number_unsigned())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, std::string* out)
{
    if  (!object.contains(name) || !object[name].is_string())
        return false;
    *out = object[name];
    return true;
}
template<typename JsonObject> inline
bool JsonReadSafe(const JsonObject& object, const char* name, bool* out)
{
    if (!object.contains(name) || !object[name].is_boolean())
        return false;
    *out = object[name];
    return true;
}

template<typename JsonObject, typename EnumT> inline
bool JsonReadSafeEnum(const JsonObject& object, const char* name, EnumT* out)
{
    std::string str;
    if (!object.contains(name) || !object[name].is_string())
        return false;

    str = object[name];
    const auto& enum_val = magic_enum::enum_cast<EnumT>(str);
    if (!enum_val.has_value())
        return false;

    *out = enum_val.value();
    return true;
}

template<typename JsonObject, typename T> inline
bool JsonReadObject(const JsonObject& object, const char* name, T* out)
{
    if (!object.contains(name) || !object[name].is_object())
        return false;
    const std::optional<T>& ret = T::FromJson(object[name]);
    if (!ret.has_value())
        return false;
    *out = ret.value();
    return true;
}

template<typename JsonObject, typename ValueT> inline
bool JsonReadSafe(const JsonObject& object, const char* name, ValueT* out)
{
    if constexpr (std::is_enum<ValueT>::value)
        return JsonReadSafeEnum(object, name, out);
    else return JsonReadObject(object, name, out);

    return false;
}

template<typename JsonObject, typename EnumT> inline
void JsonWriteEnum(JsonObject& object, const char* name, EnumT value)
{
    const std::string str(magic_enum::enum_name(value));
    object[name] = str;
}
template<typename JsonObject, typename T> inline
void JsonWriteObject(JsonObject& object, const char* name, const T& value)
{
    object[name] = value.ToJson();
}
template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, int value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, unsigned value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, double value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, float value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, const std::string& value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, bool value)
{ object[name] = value; }

template<typename JsonObject> inline
void JsonWrite(JsonObject& object, const char* name, const char* str)
{ object[name] = str; }

template<typename JsonObject, typename ValueT> inline
void JsonWrite(JsonObject& object, const char* name, const ValueT& value)
{
    if constexpr (std::is_enum<ValueT>::value)
        JsonWriteEnum(object, name, value);
    else JsonWriteObject(object, name, value);
}

} // base


#if defined(__MSVC__)
#  pragma warning(pop) // deprecated use of wstring_convert
#endif
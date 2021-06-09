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

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <fstream>

#include "base/assert.h"
#include "base/utility.h"
#include "base/json.h"
#include "data/json.h"

// Warning about nlohmann::json
// !! SEMANTICS CHANGE BETWEEN DEBUG AND RELEASE BUILD !!
//
// Trying to access an attribute using operator[] does not check
// whether a given key exists. instead it uses a standard CRT assert
// which then changes semantics depending whether NDEBUG is defined
// or not.

namespace data {

JsonObject::JsonObject(const nlohmann::json& json) : mJson(new nlohmann::json(json))
{}
JsonObject::JsonObject(nlohmann::json&& json) : mJson(new nlohmann::json(std::move(json)))
{}
JsonObject::JsonObject(std::shared_ptr<nlohmann::json> json) : mJson(json)
{}
JsonObject::JsonObject() : mJson(new nlohmann::json) {}
JsonObject::~JsonObject() = default;

std::unique_ptr<Reader> JsonObject::GetReadChunk(const char* name) const
{
    if (!mJson->contains(name))
        return nullptr;
    const auto& obj = (*mJson)[name];
    if (!obj.is_object())
        return nullptr;
    return std::make_unique<JsonObject>(obj);
}
std::unique_ptr<Reader> JsonObject::GetReadChunk(const char* name, unsigned index) const
{
    if (!mJson->contains(name))
        return nullptr;
    const auto& obj = ((*mJson)[name])[index];
    if (!obj.is_object())
        return nullptr;
    return std::make_unique<JsonObject>(obj);
}

bool JsonObject::Read(const char* name, float* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, int* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, unsigned* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, bool* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, std::string* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, glm::vec2* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, glm::vec3* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, glm::vec4* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, base::FRect* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, base::FPoint* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, base::FSize* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::Read(const char* name, base::Color4f* out) const
{
    return base::JsonReadSafe(*mJson, name, out);
}
bool JsonObject::HasValue(const char* name) const
{
    return mJson->contains(name);
}
bool JsonObject::HasChunk(const char* name) const
{
    return mJson->contains(name) && ((*mJson)[name]).is_object();
}
bool JsonObject::IsEmpty() const
{
    return mJson->empty();
}

unsigned JsonObject::GetNumChunks(const char* name) const
{
    if (mJson->contains(name) && ((*mJson)[name]).is_array())
        return (*mJson)[name].size();
    return 0;
}

std::unique_ptr<Writer> JsonObject::NewWriteChunk() const
{ return std::make_unique<JsonObject>(); }

void JsonObject::Write(const char* name, int value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, unsigned value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, double value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, float value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, bool value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const char* value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const std::string& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const glm::vec2& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const glm::vec3& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const glm::vec4& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const base::FRect& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const base::FPoint& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const base::FSize& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const base::Color4f& value)
{
    base::JsonWrite(*mJson, name, value);
}
void JsonObject::Write(const char* name, const Writer& chunk)
{
    const auto* json = dynamic_cast<const JsonObject*>(&chunk);
    ASSERT(json);
    base::detail::JsonWriteJson(*mJson, name, *json->mJson);
}
void JsonObject::Write(const char* name, std::unique_ptr<Writer> chunk)
{
    auto* json = dynamic_cast<JsonObject*>(chunk.get());
    ASSERT(json);
    base::detail::JsonWriteJson(*mJson, name, std::move(*json->mJson));
}

void JsonObject::AppendChunk(const char* name, const Writer& chunk)
{
    const auto* json = dynamic_cast<const JsonObject*>(&chunk);
    ASSERT(json);
    (*mJson)[name].push_back(*json->mJson);
}
void JsonObject::AppendChunk(const char* name, std::unique_ptr<Writer> chunk)
{
    auto* json = dynamic_cast<JsonObject*>(chunk.get());
    ASSERT(json);
    (*mJson)[name].push_back(std::move(*json->mJson));
}

std::tuple<bool, std::string> JsonObject::ParseString(const std::string& str)
{
    auto [ok, json, error] = base::JsonParse(str);
    if (!ok)
        return std::make_tuple(false, error);
    (*mJson) = std::move(json);
    return std::make_tuple(true, "");
}

std::string JsonObject::ToString() const
{ return mJson->dump(2); }

JsonFile::JsonFile(const std::string& file)
{
    auto io = base::OpenBinaryInputStream(file);
    if (!io.is_open())
        throw std::runtime_error("failed to open: " + file);
    auto json = nlohmann::json::parse(std::istreambuf_iterator<char>(io),
                                      std::istreambuf_iterator<char>{});
    mJson = std::make_shared<nlohmann::json>(std::move(json));
}
JsonFile::JsonFile() = default;
JsonFile::~JsonFile() = default;

std::tuple<bool, std::string> JsonFile::Load(const std::string& file)
{
    auto io = base::OpenBinaryInputStream(file);
    if (!io.is_open())
        return std::make_tuple(false, "failed to open: " + file);
    try
    {
        auto json = nlohmann::json::parse(std::istreambuf_iterator<char>(io),
                                          std::istreambuf_iterator<char>{});
        mJson = std::make_shared<nlohmann::json>(std::move(json));
    }
    catch (const std::exception& e)
    { return std::make_tuple(false, std::string("JSON parse error: ") + e.what()); }
    return std::make_tuple(true, "");
}

std::tuple<bool, std::string> JsonFile::Save(const std::string& file) const
{
    auto io = base::OpenBinaryOutputStream(file);
    if (!io.is_open())
        return std::make_tuple(false, "failed to open: " + file);
    const auto& string = mJson->dump(2);
    io.write(string.c_str(), string.size());
    if (io.fail())
        throw std::make_tuple(false, "JSON file write failed");
    return std::make_tuple(true, "");
}

JsonObject JsonFile::GetRootObject()
{ return JsonObject(mJson); }

void JsonFile::SetRootObject(JsonObject& object)
{ mJson = object.GetJson(); }

} // namespace

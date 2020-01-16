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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#  include <QByteArray>
#  include <QFile>
#  include <QIcon>
#include "warnpop.h"

#include "eventlog.h"
#include "workspace.h"
#include "utility.h"
#include "format.h"

namespace app
{

Workspace::Workspace()
{
    DEBUG("Create workspace");
}

Workspace::~Workspace()
{
    DEBUG("Destroy workspace");
}

QVariant Workspace::data(const QModelIndex& index, int role) const
{
    const auto& res = mResources[index.row()];

    if (role == Qt::SizeHintRole)
        return QSize(0, 16);
    else if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case 0: return toString(res->GetType());
            case 1: return res->GetName();
        }
    }
    else if (role == Qt::DecorationRole && index.column() == 0)
    {
        switch (res->GetType())
        {
            case Resource::Type::Material:
                return QIcon("icons:material.png");
            case Resource::Type::Drawable:
                return QIcon("icons:material.png");
            default: break;
        }
    }
    return QVariant();
}

QVariant Workspace::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case 0:  return "Type";
            case 1:  return "Name";
        }
    }
    return QVariant();
}

bool Workspace::LoadWorkspace(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }
    const auto& buff = file.readAll(); // QByteArray
    if (buff.isEmpty())
    {
        WARN("No workspace content found in file: '%1'", filename);
        return false;
    }

    const auto* beg = buff.data();
    const auto* end = buff.data() + buff.size();
    // todo: this can throw, use the nothrow and log error
    const auto& json = nlohmann::json::parse(beg, end);

    std::vector<std::unique_ptr<Resource>> resources;

    if (json.contains("materials"))
    {
        for (const auto& json_mat : json["materials"].items())
        {
            bool success  = false;
            auto material = gfx::Material::FromJson(json_mat.value(), &success);
            if (!success)
            {
                ERROR("Failed to load material properties.");
                continue;
            }
            DEBUG("Loaded material: '%1'", material.GetName());
            resources.push_back(std::make_unique<MaterialResource>(material));
        }
    }
    mResources = std::move(resources);
    mFilename = filename;
    return true;
}

bool Workspace::SaveWorkspace(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    nlohmann::json json;
    for (const auto& resource : mResources)
    {
        resource->Serialize(json);
    }

    if (json.is_null())
    {
        WARN("Workspace contains no actual data. Skipped saving.");
        return true;
    }

    const auto& str = json.dump(2);
    if (file.write(&str[0], str.size()) == -1)
    {
        ERROR("File write failed. '%1'", filename);
        return false;
    }
    file.flush();
    file.close();
    mFilename = filename;
    return true;
}

QStringList Workspace::ListMaterials() const
{
    QStringList list;
    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::Material)
            list.append(res->GetName());
    }
    return list;
}

QStringList Workspace::ListDrawables() const
{
    QStringList list;
    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::Drawable)
            list.append(res->GetName());
    }
    return list;
}

void Workspace::SaveMaterial(const gfx::Material& material)
{
    // check if we already have on by this name.
    // the caller is expected to have confirmed the user if overwriting
    // is OK or not.
    const auto& name = app::fromUtf8(material.GetName());

    for (size_t i=0; i<mResources.size(); ++i)
    {
        const auto& res = mResources[i];
        if (res->GetType() == Resource::Type::Material &&
            res->GetName() == name)
        {
            mResources[i] = std::make_unique<MaterialResource>(material);
            return;
        }
    }
    beginInsertRows(QModelIndex(), mResources.size() + 1,
        mResources.size() + 1);
    mResources.push_back(std::make_unique<MaterialResource>(material));
    endInsertRows();
}

bool Workspace::HasMaterial(const QString& name) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == Resource::Type::Material &&
            res->GetName() == name)
            return true;
    }
    return false;
}

} // namespace

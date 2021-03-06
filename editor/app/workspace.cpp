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

#define LOGTAG "workspace"

#include "config.h"

#include "warnpush.h"
#  include <boost/algorithm/string/erase.hpp>
#  include <nlohmann/json.hpp>
#  include <neargye/magic_enum.hpp>
#  include <private/qfsfileengine_p.h> // private in Qt5
#  include <QCoreApplication>
#  include <QtAlgorithms>
#  include <QJsonDocument>
#  include <QJsonArray>
#  include <QByteArray>
#  include <QFile>
#  include <QFileInfo>
#  include <QIcon>
#  include <QPainter>
#  include <QImage>
#  include <QImageWriter>
#  include <QPixmap>
#  include <QDir>
#  include <QStringList>
#include "warnpop.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <set>
#include <functional>

#include "editor/app/eventlog.h"
#include "editor/app/workspace.h"
#include "editor/app/utility.h"
#include "editor/app/format.h"
#include "editor/app/packing.h"
#include "editor/app/buffer.h"
#include "graphics/resource.h"
#include "graphics/color4f.h"
#include "engine/ui.h"
#include "engine/data.h"
#include "data/json.h"
#include "base/json.h"

namespace {

gfx::Color4f ToGfx(const QColor& color)
{
    const float a  = color.alphaF();
    const float r  = color.redF();
    const float g  = color.greenF();
    const float b  = color.blueF();
    return gfx::Color4f(r, g, b, a);
}

QString GetAppDir()
{
    static const auto& dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
#if defined(POSIX_OS)
    return dir + "/";
#elif defined(WINDOWS_OS)
    return dir + "\\";
#else
# error unimplemented function
#endif
    return dir;
}

class ResourcePacker : public gfx::ResourcePacker
{
public:
    using ObjectHandle = gfx::ResourcePacker::ObjectHandle;

    ResourcePacker(const QString& outdir, unsigned max_width, unsigned max_height, unsigned padding, bool resize_large, bool pack_small)
        : kOutDir(outdir)
        , kMaxTextureWidth(max_width)
        , kMaxTextureHeight(max_height)
        , kTexturePadding(padding)
        , kResizeLargeTextures(resize_large)
        , kPackSmallTextures(pack_small)
    {}

    virtual void PackShader(ObjectHandle instance, const std::string& file) override
    {
        // todo: the shader type/path (es2 or 3)
        mShaderMap[instance] = CopyFile(file, "shaders/es2");
    }
    virtual void PackTexture(ObjectHandle instance, const std::string& file) override
    {
        mTextureMap[instance].file = file;
    }
    virtual void SetTextureBox(ObjectHandle instance, const gfx::FRect& box) override
    {
        mTextureMap[instance].rect = box;
    }
    virtual void SetTextureFlag(ObjectHandle instance, gfx::ResourcePacker::TextureFlags flags, bool on_off) override
    {
        ASSERT(flags == gfx::ResourcePacker::TextureFlags::CanCombine);
        mTextureMap[instance].can_be_combined = on_off;
    }
    virtual void PackFont(ObjectHandle instance, const std::string& file) override
    {
        mFontMap[instance] = CopyFile(file, "fonts");
    }

    virtual std::string GetPackedShaderId(ObjectHandle instance) const override
    {
        auto it = mShaderMap.find(instance);
        ASSERT(it != std::end(mShaderMap));
        return it->second;
    }
    virtual std::string GetPackedTextureId(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.file;
    }
    virtual std::string GetPackedFontId(ObjectHandle instance) const override
    {
        auto it = mFontMap.find(instance);
        ASSERT(it != std::end(mFontMap));
        return it->second;
    }
    virtual gfx::FRect GetPackedTextureBox(ObjectHandle instance) const override
    {
        auto it = mTextureMap.find(instance);
        ASSERT(it != std::end(mTextureMap));
        return it->second.rect;
    }

    using TexturePackingProgressCallback = std::function<void (std::string, int, int)>;

    void PackTextures(TexturePackingProgressCallback  progress)
    {
        if (mTextureMap.empty())
            return;

        if (!app::MakePath(app::JoinPath(kOutDir, "textures")))
        {
            ERROR("Failed to create %1/%2", kOutDir, "textures");
            mNumErrors++;
            return;
        }

        // OpenGL ES 2. defines the minimum required supported texture size to be
        // only 64x64 px which is not much. Anything bigger than that
        // is implementation specific. :p
        // for maximum portability we then just pretty much skip the whole packing.

        // the source list of rectangles (images to pack)
        std::vector<app::PackingRectangle> sources;

        struct GeneratedTextureEntry {
            QString  texture_file;
            // box of the texture that was packed
            // now inside the texture_file
            float xpos   = 0;
            float ypos   = 0;
            float width  = 0;
            float height = 0;
        };

        // map original file handle to a new generated texture entry
        // which defines either a box inside a generated texture pack
        // (combination of multiple textures) or a downsampled
        // (originally large) texture.
        std::unordered_map<std::string, GeneratedTextureEntry> relocation_map;

        // duplicate source file entries are discarded.
        std::set<std::string> dupemap;

        // 1. go over the list of textures, ignore duplicates
        // 2. select textures that seem like a good "fit" for packing. (size ?)
        // 3. combine the textures into atlas/atlasses.
        // -- composite the actual image files.
        // 4. copy the src image contents into the container image.
        // 5. write the container/packed image into the package folder
        // 6. update the textures whose source images were packaged (the file handle and the rectangle box)

        int cur_step = 0;
        int max_step = static_cast<int>(mTextureMap.size());

        for (auto it = mTextureMap.begin(); it != mTextureMap.end(); ++it)
        {
            progress("Copying textures...", cur_step++, max_step);

            const TextureSource& tex = it->second;
            if (tex.file.empty())
                continue;
            else if (auto it = dupemap.find(tex.file) != dupemap.end())
                continue;

            const QFileInfo info(app::FromUtf8(tex.file));
            const QString src_file = info.absoluteFilePath();
            const QPixmap src_pix(src_file);
            if (src_pix.isNull())
            {
                ERROR("Failed to open image: '%1'", src_file);
                mNumErrors++;
                continue;
            }

            const auto width  = src_pix.width();
            const auto height = src_pix.height();
            DEBUG("Image %1 %2x%3 px", src_file, width, height);
            if (width >= kMaxTextureWidth || height >= kMaxTextureHeight)
            {
                QString filename;
                if ((width > kMaxTextureWidth || height > kMaxTextureHeight) && kResizeLargeTextures)
                {
                    auto it = mResourceMap.find(tex.file);
                    if (it != mResourceMap.end())
                    {
                        DEBUG("Skipping duplicate copy of '%1'", tex.file);
                        filename = app::FromUtf8(it->second);
                    }
                    else
                    {
                        const auto scale = std::min(kMaxTextureWidth / (float) width,
                                                    kMaxTextureHeight / (float) height);
                        const auto dst_width = width * scale;
                        const auto dst_height = height * scale;
                        QPixmap buffer(dst_width, dst_height);
                        buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));
                        QPainter painter(&buffer);
                        painter.setCompositionMode(QPainter::CompositionMode_Source);
                        const QRectF dst_rect(0, 0, dst_width, dst_height);
                        const QRectF src_rect(0, 0, width, height);
                        painter.drawPixmap(dst_rect, src_pix, src_rect);

                        // create a scratch file into which write the re-sampled image file
                        // and then copy from here to the packing location. This lets the
                        // file name collision mapping to work as-is.
                        const QString& name = info.baseName() + ".png";
                        const QString temp = app::JoinPath(QDir::tempPath(), name);
                        QImageWriter writer;
                        writer.setFormat("PNG");
                        writer.setQuality(100);
                        writer.setFileName(temp);
                        if (!writer.write(buffer.toImage()))
                        {
                            ERROR("Failed to write re-sampling temp image '%1'", temp);
                            mNumErrors++;
                            continue;
                        }
                        auto pckid = CopyFile(app::ToUtf8(temp), "textures");
                        DEBUG("Texture '%1' (%2x%3px) was re-sampled.", src_file, width, height);
                        // a bit of a hack to have this code here.. but strike the cached
                        // temp file path from the map of already copied and replace with the
                        // actual *source* image name which is what we really want to be using.
                        mResourceMap.erase(app::ToUtf8(temp));
                        mResourceMap[tex.file] = pckid;
                        filename = app::FromUtf8(pckid);
                    }
                }
                else
                {
                    // copy the file as is, since it's just at the max allowed
                    // texture size.
                    filename = app::FromUtf8(CopyFile(tex.file , "textures"));
                }

                GeneratedTextureEntry self;
                self.width = 1.0f;
                self.height = 1.0f;
                self.xpos = 0.0f;
                self.ypos = 0.0f;
                self.texture_file = filename;
                relocation_map[tex.file] = std::move(self);
            }
            else if (!kPackSmallTextures || !tex.can_be_combined)
            {
                // add as an identity texture relocation entry.
                GeneratedTextureEntry self;
                self.width  = 1.0f;
                self.height = 1.0f;
                self.xpos   = 0.0f;
                self.ypos   = 0.0f;
                self.texture_file   = app::FromUtf8(CopyFile(tex.file, "textures"));
                relocation_map[tex.file] = std::move(self);
            }
            else
            {
                // add as a source for texture packing
                app::PackingRectangle rc;
                rc.width  = src_pix.width() + kTexturePadding * 2;
                rc.height = src_pix.height() + kTexturePadding * 2;
                rc.cookie = tex.file;
                sources.push_back(rc);
            }

            dupemap.insert(tex.file);
        }

        unsigned atlas_number = 0;
        cur_step = 0;
        max_step = static_cast<int>(sources.size());

        while (!sources.empty())
        {
            progress("Packing textures...", cur_step++, max_step);

            app::RectanglePackSize packing_rect_result;
            app::PackRectangles({kMaxTextureWidth, kMaxTextureHeight}, sources, &packing_rect_result);
            // ok, some of the textures might have failed to pack on this pass.
            // separate the ones that were successfully packed from the ones that
            // weren't. then composite the image for the success cases.
            auto first_success = std::partition(sources.begin(), sources.end(),
                [](const auto& pack_rect) {
                    // put the failed cases first.
                    return pack_rect.success == false;
                });
            const auto num_to_pack = std::distance(first_success, sources.end());
            // we should have already dealt with too big images already.
            ASSERT(num_to_pack > 0);
            if (num_to_pack == 1)
            {
                // if we can only fit 1 single image in the container
                // then what's the point ?
                // we'd just end up wasting space, so just leave it as is.
                const auto& rc = *first_success;
                GeneratedTextureEntry gen;
                gen.texture_file = app::FromUtf8(CopyFile(rc.cookie, "textures"));
                gen.width  = 1.0f;
                gen.height = 1.0f;
                gen.xpos   = 0.0f;
                gen.ypos   = 0.0f;
                relocation_map[rc.cookie] = gen;
                sources.erase(first_success);
                continue;
            }


            // composition buffer.
            QPixmap buffer(packing_rect_result.width, packing_rect_result.height);
            buffer.fill(QColor(0x00, 0x00, 0x00, 0x00));

            QPainter painter(&buffer);
            painter.setCompositionMode(QPainter::CompositionMode_Source); // copy src pixel as-is

            // do the composite pass.
            for (auto it = first_success; it != sources.end(); ++it)
            {
                const auto& rc = *it;
                ASSERT(rc.success);
                const auto padded_width  = rc.width;
                const auto padded_height = rc.height;
                const auto width  = padded_width - kTexturePadding*2;
                const auto height = padded_height - kTexturePadding*2;

                const QFileInfo info(app::FromUtf8(rc.cookie));
                const QString file(info.absoluteFilePath());
                // compensate for possible texture sampling issues by padding the
                // image with some extra pixels by growing it a few pixels on both
                // axis.
                const QRectF dst(rc.xpos, rc.ypos, padded_width, padded_height);
                const QRectF src(0, 0, width, height);
                const QPixmap pix(file);
                if (pix.isNull())
                {
                    ERROR("Failed to open image: '%1'", file);
                    mNumErrors++;
                }
                else
                {
                    painter.drawPixmap(dst, pix, src);
                }
            }

            const QString& name = QString("Generated_%1.png").arg(atlas_number);
            const QString& file = app::JoinPath(app::JoinPath(kOutDir, "textures"), name);

            QImageWriter writer;
            writer.setFormat("PNG");
            writer.setQuality(100);
            writer.setFileName(file);
            if (!writer.write(buffer.toImage()))
            {
                ERROR("Failed to write image '%1'", file);
                mNumErrors++;
            }
            const float pack_width  = packing_rect_result.width;
            const float pack_height = packing_rect_result.height;

            // create mapping for each source texture to the generated
            // texture.
            for (auto it = first_success; it != sources.end(); ++it)
            {
                const auto& rc = *it;
                const auto padded_width  = rc.width;
                const auto padded_height = rc.height;
                const auto width  = padded_width - kTexturePadding*2;
                const auto height = padded_height - kTexturePadding*2;
                const auto xpos   = rc.xpos + kTexturePadding;
                const auto ypos   = rc.ypos + kTexturePadding;
                GeneratedTextureEntry gen;
                gen.texture_file   = QString("pck://textures/%1").arg(name);
                gen.width          = (float)width / pack_width;
                gen.height         = (float)height / pack_height;
                gen.xpos           = (float)xpos / pack_width;
                gen.ypos           = (float)ypos / pack_height;
                relocation_map[rc.cookie] = gen;
                DEBUG("Packed %1 into %2", rc.cookie, gen.texture_file);
            }

            // done with these.
            sources.erase(first_success, sources.end());

            atlas_number++;
        }
        cur_step = 0;
        max_step = static_cast<int>(mTextureMap.size());
        // update texture object mappings, file handles and texture boxes.
        // for each texture object, look up where the original file handle
        // maps to. Then the original texture box is now a box within a box.
        for (auto& pair : mTextureMap)
        {
            progress("Remapping textures...", cur_step++, max_step);

            TextureSource& tex = pair.second;
            const auto original_file = tex.file;
            const auto original_rect = tex.rect;

            auto it = relocation_map.find(original_file);
            if (it == relocation_map.end()) // font texture sources only have texture box.
                continue;
            //ASSERT(it != relocation_map.end());
            const GeneratedTextureEntry& relocation = it->second;

            const auto original_rect_x = original_rect.GetX();
            const auto original_rect_y = original_rect.GetY();
            const auto original_rect_width  = original_rect.GetWidth();
            const auto original_rect_height = original_rect.GetHeight();

            tex.file = app::ToUtf8(relocation.texture_file);
            tex.rect = gfx::FRect(relocation.xpos + original_rect_x * relocation.width,
                                  relocation.ypos + original_rect_y * relocation.height,
                                  relocation.width * original_rect_width,
                                  relocation.height * original_rect_height);
        }
    }


    size_t GetNumErrors() const
    { return mNumErrors; }
    size_t GetNumFilesCopied() const
    { return mNumFilesCopied; }

    std::string CopyFile(const std::string& file, const QString& where)
    {
        auto it = mResourceMap.find(file);
        if (it != std::end(mResourceMap))
        {
            DEBUG("Skipping duplicate copy of '%1'", file);
            return it->second;
        }

        if (!app::MakePath(app::JoinPath(kOutDir, where)))
        {
            ERROR("Failed to create '%1/%2'", kOutDir, where);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        // this will actually resolve the file path.
        // using the resolution scheme in Workspace.
        const QFileInfo src_info(app::FromUtf8(file));
        if (!src_info.exists())
        {
            ERROR("File %1 could not be found!", file);
            mNumErrors++;
            // todo: what to return on error ?
            return "";
        }

        const QString& src_file = src_info.absoluteFilePath(); // resolved path.
        const QString& src_name = src_info.fileName();
        QString dst_name = src_name;
        QString dst_file = app::JoinPath(kOutDir, app::JoinPath(where, dst_name));
        // try to generate a different name for the file when a file
        // by the same name already exists.
        unsigned attempt = 0;
        while (true)
        {
            const QFileInfo dst_info(dst_file);
            // if there's no file by this name we're good to go
            if (!dst_info.exists())
                break;
            // if the destination file exists *from before* we're
            // going to overwrite it. The user should have been confirmed
            // for this and it should be fine at this point.
            // So only try to resolve a name collision if we're trying to
            // write an output file by the same name multiple times
            if (mFileNames.find(dst_file) == mFileNames.end())
                break;
            // generate a new name.
            dst_name = QString("%1_%2").arg(attempt).arg(src_name);
            dst_file = app::JoinPath(kOutDir, app::JoinPath(where, dst_name));
            attempt++;
        }
        CopyFileBuffer(src_file, dst_file);
        // keep track of which files we wrote.
        mFileNames.insert(dst_file);

        // generate the resource identifier
        const auto& pckid = app::ToUtf8(QString("pck://%1/%2").arg(where).arg(dst_name));

        mResourceMap[file] = pckid;
        return pckid;
    }
private:
    void CopyFileBuffer(const QString& src, const QString& dst)
    {
        // if src equals dst then we can actually skip the copy, no?
        if (src == dst)
        {
            DEBUG("Skipping copy of '%1' to '%2'", src, dst);
            return;
        }

        // we're doing this silly copying here since Qt doesn't
        // have a copy operation that's without race condition,
        // i.e. QFile::copy won't overwrite.
        QFile src_io(src);
        QFile dst_io(dst);
        if (!src_io.open(QIODevice::ReadOnly))
        {
            ERROR("Failed to open '%1' for reading (%2).", src, src_io.error());
            mNumErrors++;
            return;
        }
        if (!dst_io.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            ERROR("Failed to open '%1 for writing (%2).", dst, dst_io.error());
            mNumErrors++;
            return;
        }
        const auto& buffer = src_io.readAll();
        if (dst_io.write(buffer) == -1)
        {
            ERROR("Failed to write file '%1' (%2)", dst, dst_io.error());
            mNumErrors++;
            return;
        }
        dst_io.setPermissions(src_io.permissions());
        mNumFilesCopied++;
        DEBUG("Copied %1 bytes from %2 to %3", buffer.count(), src, dst);
    }
private:
    const QString kOutDir;
    const unsigned kMaxTextureHeight = 0;
    const unsigned kMaxTextureWidth = 0;
    const unsigned kTexturePadding = 0;
    const bool kResizeLargeTextures = true;
    const bool kPackSmallTextures = true;
    std::size_t mNumErrors = 0;
    std::size_t mNumFilesCopied = 0;

    // maps from object to shader.
    std::unordered_map<ObjectHandle, std::string> mShaderMap;
    // maps from object to font.
    std::unordered_map<ObjectHandle, std::string> mFontMap;

    struct TextureSource {
        std::string file;
        gfx::FRect  rect;
        bool can_be_combined = true;
    };
    std::unordered_map<ObjectHandle, TextureSource> mTextureMap;

    // resource mapping from source to packed resource id.
    std::unordered_map<std::string, std::string> mResourceMap;
    // filenames of files we've written.
    std::unordered_set<QString> mFileNames;
};

} // namespace

namespace app
{

Workspace::Workspace()
{
    DEBUG("Create workspace");

    // initialize the primitive resources, i.e the materials
    // and drawables that are part of the workspace without any
    // user interaction.

    // Checkerboard is a special material that is always available.
    // It is used as the initial material when user hasn't selected
    // anything or when the material referenced by some object is deleted
    // the material reference can be updated to Checkerboard.
    auto checkerboard = gfx::CreateMaterialFromTexture("app://textures/Checkerboard.png");
    checkerboard.SetId("_checkerboard");
    mResources.emplace_back(new MaterialResource(std::move(checkerboard), "Checkerboard"));

    // add some primitive colors.
    constexpr auto& values = magic_enum::enum_values<gfx::Color>();
    for (const auto& val : values)
    {
        const std::string color_name(magic_enum::enum_name(val));
        auto color = gfx::CreateMaterialFromColor(gfx::Color4f(val));
        color.SetId("_" + color_name);
        mResources.emplace_back(new MaterialResource(std::move(color), FromUtf8(color_name)));
    }

    mResources.emplace_back(new DrawableResource<gfx::CapsuleClass>("Capsule"));
    mResources.emplace_back(new DrawableResource<gfx::RectangleClass>("Rectangle"));
    mResources.emplace_back(new DrawableResource<gfx::IsoscelesTriangleClass>("IsoscelesTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::RightTriangleClass>("RightTriangle"));
    mResources.emplace_back(new DrawableResource<gfx::CircleClass>("Circle"));
    mResources.emplace_back(new DrawableResource<gfx::SemiCircleClass>("SemiCircle"));
    mResources.emplace_back(new DrawableResource<gfx::TrapezoidClass>("Trapezoid"));
    mResources.emplace_back(new DrawableResource<gfx::ParallelogramClass>("Parallelogram"));
    {
        gfx::RoundRectangleClass klass("_round_rect", 0.05f);
        mResources.emplace_back(new DrawableResource<gfx::RoundRectangleClass>(klass, "RoundRect"));
    }

    for (auto& resource : mResources)
    {
        resource->SetIsPrimitive(true);
    }
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
        return res->GetIcon();
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

QAbstractFileEngine* Workspace::create(const QString& file) const
{
    // CAREFUL ABOUT RECURSION HERE.
    // DO NOT CALL QFile, QFileInfo or QDir !

    // only handle our special cases.
    QString ret = file;
    if (ret.startsWith("ws://"))
        ret.replace("ws://", mWorkspaceDir);
    else if (file.startsWith("app://"))
        ret.replace("app://", GetAppDir());
    else if (file.startsWith("fs://"))
        ret.remove(0, 5);
    else return nullptr;

    DEBUG("Mapping Qt file '%1' => '%2'", file, ret);

    return new QFSFileEngine(ret);
}

std::unique_ptr<gfx::Material> Workspace::MakeMaterialByName(const QString& name) const
{
    return gfx::CreateMaterialInstance(GetMaterialClassByName(name));

}
std::unique_ptr<gfx::Drawable> Workspace::MakeDrawableByName(const QString& name) const
{
    return gfx::CreateDrawableInstance(GetDrawableClassByName(name));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const QString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassByName(const char* name) const
{
    // convenience shim
    return GetMaterialClassByName(QString::fromUtf8(name));
}

std::shared_ptr<const gfx::MaterialClass> Workspace::GetMaterialClassById(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    BUG("No such material class.");
    return nullptr;
}

std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const QString& name) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetName() != name)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassByName(const char* name) const
{
    // convenience shim
    return GetDrawableClassByName(QString::fromUtf8(name));
}
std::shared_ptr<const gfx::DrawableClass> Workspace::GetDrawableClassById(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (!(resource->GetType() == Resource::Type::Shape ||
              resource->GetType() == Resource::Type::ParticleSystem ||
              resource->GetType() == Resource::Type::Drawable))
            continue;
        else if (resource->GetId() != id)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    BUG("No such drawable class.");
    return nullptr;
}


std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassByName(const QString& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetName() != name)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}
std::shared_ptr<const game::EntityClass> Workspace::GetEntityClassById(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetId() != id)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    BUG("No such entity class.");
    return nullptr;
}

game::ClassHandle<const uik::Window> Workspace::FindUIByName(const std::string& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::UI)
            continue;
        else if (resource->GetNameUtf8() != name)
            continue;
        return ResourceCast<uik::Window>(*resource).GetSharedResource();
    }
    return nullptr;
}
game::ClassHandle<const uik::Window> Workspace::FindUIById(const std::string& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::UI)
            continue;
        else if (resource->GetIdUtf8() != id)
            continue;
        return ResourceCast<uik::Window>(*resource).GetSharedResource();
    }
    return nullptr;
}

game::ClassHandle<const gfx::MaterialClass> Workspace::FindMaterialClassById(const std::string& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Material)
            continue;
        else if (resource->GetIdUtf8() != klass)
            continue;
        return ResourceCast<gfx::MaterialClass>(*resource).GetSharedResource();
    }
    return nullptr;
}

game::ClassHandle<const gfx::DrawableClass> Workspace::FindDrawableClassById(const std::string& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetIdUtf8() != klass)
            continue;

        if (resource->GetType() == Resource::Type::Drawable)
            return ResourceCast<gfx::DrawableClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::ParticleSystem)
            return ResourceCast<gfx::KinematicsParticleEngineClass>(*resource).GetSharedResource();
        else if (resource->GetType() == Resource::Type::Shape)
            return ResourceCast<gfx::PolygonClass>(*resource).GetSharedResource();
    }
    return nullptr;
}

game::ClassHandle<const game::EntityClass> Workspace::FindEntityClassByName(const std::string& name) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetNameUtf8() != name)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    return nullptr;
}
game::ClassHandle<const game::EntityClass> Workspace::FindEntityClassById(const std::string& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Entity)
            continue;
        else if (resource->GetIdUtf8() != id)
            continue;
        return ResourceCast<game::EntityClass>(*resource).GetSharedResource();
    }
    return nullptr;
}

game::ClassHandle<const game::SceneClass> Workspace::FindSceneClassByName(const std::string& name) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetNameUtf8() != name)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
        break;
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& node = ret->GetNode(i);
        auto klass = FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            const auto& node_id    = node.GetId();
            const auto& node_name  = node.GetName();
            const auto& node_entity_id = node.GetEntityId();
            WARN("Scene '%1' node '%2' ('%2') refers to entity '%3' that is not found.",
                 name, node_id, node_name, node_entity_id);
        }
        else
        {
            node.SetEntity(klass);
        }
    }
    return ret;
}
game::ClassHandle<const game::SceneClass> Workspace::FindSceneClassById(const std::string& id) const
{
    std::shared_ptr<game::SceneClass> ret;
    for (const auto& resource : mResources)
    {
        if (resource->GetType() != Resource::Type::Scene)
            continue;
        else if (resource->GetIdUtf8() != id)
            continue;
        ret = ResourceCast<game::SceneClass>(*resource).GetSharedResource();
    }
    if (!ret)
        return nullptr;

    // resolve entity references.
    for (size_t i=0; i<ret->GetNumNodes(); ++i)
    {
        auto& node = ret->GetNode(i);
        auto klass = FindEntityClassById(node.GetEntityId());
        if (!klass)
        {
            const auto& node_id    = node.GetId();
            const auto& node_name  = node.GetName();
            const auto& node_entity_id = node.GetEntityId();
            WARN("Scene '%1' node '%2' ('%3') refers to entity '%3' that is not found.",
                 id, node_id, node_name, node_entity_id);
        }
        else
        {
            node.SetEntity(klass);
        }
    }
    return ret;
}

game::GameDataHandle Workspace::LoadGameData(const std::string& URI)
{
    const auto& file = MapFileToFilesystem(app::FromUtf8(URI));
    DEBUG("URI '%1' => '%2'", URI, file);
    return GameDataFileBuffer::LoadFromFile(file);
}

gfx::ResourceHandle Workspace::LoadResource(const std::string& URI)
{
    const auto& file = MapFileToFilesystem(app::FromUtf8(URI));
    DEBUG("URI '%1' => '%2'", URI, file);
    return GraphicsFileBuffer::LoadFromFile(file);
}

bool Workspace::LoadWorkspace(const QString& dir)
{
    ASSERT(!mIsOpen);

    if (!LoadContent(JoinPath(dir, "content.json")) ||
        !LoadProperties(JoinPath(dir, "workspace.json")))
        return false;

    // we don't really care if this fails or not. nothing permanently
    // important should be stored in this file. I.e deleting it
    // will just make the application forget some data that isn't
    // crucial for the operation of the application or for the
    // integrity of the workspace and its content.
    LoadUserSettings(JoinPath(dir, ".workspace_private.json"));

    INFO("Loaded workspace '%1'", dir);
    mWorkspaceDir = CleanPath(dir);
#if defined(POSIX_OS)
    if (!mWorkspaceDir.endsWith("/"))
        mWorkspaceDir.append("/");
#elif defined(WINDOWS_OS)
    if (!mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("\\");
#endif
    mIsOpen = true;
    return true;
}

bool Workspace::MakeWorkspace(const QString& directory)
{
    ASSERT(!mIsOpen);

    QDir dir;
    if (!dir.mkpath(directory))
    {
        ERROR("Failed to create workspace directory. '%1'", directory);
        return false;
    }

    // this is where we could initialize the workspace with some resources
    // or whatnot.
    mWorkspaceDir = CleanPath(directory);
#if defined(POSIX_OS)
    if (!mWorkspaceDir.endsWith("/"))
        mWorkspaceDir.append("/");
#elif defined(WINDOWS_OS)
    if (!mWorkspaceDir.endsWith("\\"))
        mWorkspaceDir.append("\\");
#endif
    mIsOpen = true;
    return true;
}

bool Workspace::SaveWorkspace()
{
    ASSERT(!mWorkspaceDir.isEmpty());

    if (!SaveContent(JoinPath(mWorkspaceDir, "content.json")) ||
        !SaveProperties(JoinPath(mWorkspaceDir, "workspace.json")))
        return false;

    // should we notify the user if this fails or do we care?
    SaveUserSettings(JoinPath(mWorkspaceDir, ".workspace_private.json"));

    INFO("Saved workspace '%1'", mWorkspaceDir);
    return true;
}

void Workspace::CloseWorkspace()
{
    // remove all non-primitive resources.
    QAbstractTableModel::beginResetModel();
    mResources.erase(std::remove_if(mResources.begin(), mResources.end(),
                                    [](const auto& res) { return !res->IsPrimitive(); }),
                     mResources.end());
    QAbstractTableModel::endResetModel();

    mVisibleCount = 0;

    mProperties.clear();
    mUserProperties.clear();
    mWorkspaceDir.clear();
    mSettings = ProjectSettings {};
    mIsOpen   = false;
}

QString Workspace::GetName() const
{
    return mWorkspaceDir;
}

QString Workspace::MapFileToWorkspace(const QString& filepath) const
{
    // don't remap already mapped files.
    if (filepath.startsWith("app://") ||
        filepath.startsWith("fs://") ||
        filepath.startsWith("ws://"))
        return filepath;

    // when the user is adding resource files to this project (workspace) there's the problem
    // of making the workspace "portable".
    // Portable here means two things:
    // * portable from one operating system to another (from Windows to Linux or vice versa)
    // * portable from one user's environment to another user's environment even when on
    //   the same operating system (i.e. from Windows to Windows or Linux to Linux)
    //
    // Using relative file paths (as opposed to absolute file paths) solves the problem
    // but then the problem is that the relative paths need to be resolved at runtime
    // and also some kind of "landmark" is needed in order to make the files
    // relative something to.
    //
    // The expectation is that during content creation most of the game resources
    // would be placed in a place relative to the workspace. In this case the
    // runtime path resolution would use paths relative to the current workspace.
    // However there's also some content (such as the pre-built shaders) that
    // is bundled with the Editor application and might be used from that location
    // as a game resource.

    // We could always copy the files into some location under the workspace to
    // solve this problem but currently this is not yet done.

    // if the file is in the current workspace path or in the path of the current executable
    // we can then express this path as a relative path.
    const auto& appdir = GetAppDir();
    const auto& file = CleanPath(filepath);

    if (file.startsWith(mWorkspaceDir))
    {
        QString ret = file;
        ret.remove(0, mWorkspaceDir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("ws://%1").arg(ret);
    }
    else if (file.startsWith(appdir))
    {
        QString ret = file;
        ret.remove(0, appdir.count());
        if (ret.startsWith("/") || ret.startsWith("\\"))
            ret.remove(0, 1);
        ret = ret.replace("\\", "/");
        return QString("app://%1").arg(ret);
    }
    // mapping other paths to identity. will not be portable to another
    // user's computer to another system, unless it's accessible on every
    // machine using the same path (for example a shared file system mount)
    return QString("fs://%1").arg(file);
}

// convenience wrapper
std::string Workspace::MapFileToWorkspace(const std::string& file) const
{
    return ToUtf8(MapFileToWorkspace(FromUtf8(file)));
}

QString Workspace::MapFileToFilesystem(const QString& uri) const
{
    // see comments in AddFileToWorkspace.
    // this is basically the same as MapFilePath except this API
    // is internal to only this application whereas MapFilePath is part of the
    // API exposed to the graphics/ subsystem.

    QString ret = uri;
    if (ret.startsWith("ws://"))
        ret = CleanPath(ret.replace("ws://", mWorkspaceDir));
    else if (uri.startsWith("app://"))
        ret = CleanPath(ret.replace("app://", GetAppDir()));
    else if (uri.startsWith("fs://"))
        ret.remove(0, 5);

    // return as is
    return ret;
}

QString Workspace::MapFileToFilesystem(const std::string& uri) const
{
    return MapFileToFilesystem(FromUtf8(uri));
}

template<typename ClassType>
void LoadResources(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<Resource>>& vector)
{
    DEBUG("Loading %1", type);
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            continue;
        }
        std::optional<ClassType> ret = ClassType::FromJson(*chunk);
        if (!ret.has_value())
        {
            ERROR("Failed to load resource '%1'", name);
            continue;
        }
        vector.push_back(std::make_unique<GameResource<ClassType>>(std::move(ret.value()), FromUtf8(name)));
        DEBUG("Loaded resource '%1'", name);
    }
}

template<typename ClassType>
void LoadMaterials(const char* type,
                   const data::Reader& data,
                   std::vector<std::unique_ptr<Resource>>& vector)
{
    DEBUG("Loading %1", type);
    for (unsigned i=0; i<data.GetNumChunks(type); ++i)
    {
        const auto& chunk = data.GetReadChunk(type, i);
        std::string name;
        std::string id;
        if (!chunk->Read("resource_name", &name) ||
            !chunk->Read("resource_id", &id))
        {
            ERROR("Unexpected JSON. Maybe old workspace version?");
            continue;
        }
        auto ret = ClassType::FromJson(*chunk);
        if (!ret)
        {
            ERROR("Failed to load resource '%1'", name);
            continue;
        }
        //vector.push_back(std::make_unique<GameResource<ClassType>>(std::move(ret), FromUtf8(name)));
        vector.push_back(std::make_unique<MaterialResource>(std::move(ret), FromUtf8(name)));
        DEBUG("Loaded resource '%1'", name);
    }
}

bool Workspace::LoadContent(const QString& filename)
{
    data::JsonFile file;
    const auto [ok, error] = file.Load(app::ToUtf8(filename));
    if (!ok)
    {
        ERROR("Failed to load JSON content file '%1'", filename);
        ERROR("JSON file load error '%1'", error);
        return false;
    }
    data::JsonObject root = file.GetRootObject();

    LoadMaterials<gfx::MaterialClass>("materials", root, mResources);
    LoadResources<gfx::KinematicsParticleEngineClass>("particles", root, mResources);
    LoadResources<gfx::PolygonClass>("shapes", root, mResources);
    LoadResources<game::EntityClass>("entities", root, mResources);
    LoadResources<game::SceneClass>("scenes", root, mResources);
    LoadResources<Script>("scripts", root, mResources);
    LoadResources<DataFile>("data_files", root, mResources);
    LoadResources<AudioFile>("audio_files", root, mResources);
    LoadResources<uik::Window>("uis", root, mResources);

    // setup an invariant that states that the primitive materials
    // are in the list of resources after the user defined ones.
    // this way the the addressing scheme (when user clicks on an item
    // in the list of resources) doesn't need to change and it's possible
    // to easily limit the items to be displayed only to those that are
    // user defined.
    auto primitives_start = std::stable_partition(mResources.begin(), mResources.end(),
        [](const auto& resource)  {
            return resource->IsPrimitive() == false;
        });
    mVisibleCount = std::distance(mResources.begin(), primitives_start);

    INFO("Loaded content file '%1'", filename);
    return true;
}

bool Workspace::SaveContent(const QString& filename) const
{
    data::JsonObject root;
    for (const auto& resource : mResources)
    {
        // skip persisting primitive resources since they're always
        // created as part of the workspace creation and their resource
        // IDs are fixed.
        if (resource->IsPrimitive())
            continue;
        // serialize the user defined resource.
        resource->Serialize(root);
    }
    data::JsonFile file;
    file.SetRootObject(root);
    const auto [ok, error] = file.Save(app::ToUtf8(filename));
    if (!ok)
    {
        ERROR("Failed to save JSON content file '%1'", filename);
        return false;
    }
    INFO("Saved workspace content in '%1'", filename);
    return true;
}

bool Workspace::SaveProperties(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    // our JSON root object
    QJsonObject json;

    QJsonObject project;
    JsonWrite(project, "multisample_sample_count", mSettings.multisample_sample_count);
    JsonWrite(project, "application_name"        , mSettings.application_name);
    JsonWrite(project, "application_version"     , mSettings.application_version);
    JsonWrite(project, "application_library_win" , mSettings.application_library_win);
    JsonWrite(project, "application_library_lin" , mSettings.application_library_lin);
    JsonWrite(project, "default_min_filter"      , mSettings.default_min_filter);
    JsonWrite(project, "default_mag_filter"      , mSettings.default_mag_filter);
    JsonWrite(project, "window_mode"             , mSettings.window_mode);
    JsonWrite(project, "window_width"            , mSettings.window_width);
    JsonWrite(project, "window_height"           , mSettings.window_height);
    JsonWrite(project, "window_can_resize"       , mSettings.window_can_resize);
    JsonWrite(project, "window_has_border"       , mSettings.window_has_border);
    JsonWrite(project, "window_vsync"            , mSettings.window_vsync);
    JsonWrite(project, "window_cursor"           , mSettings.window_cursor);
    JsonWrite(project, "config_srgb"             , mSettings.config_srgb);
    JsonWrite(project, "ticks_per_second"        , mSettings.ticks_per_second);
    JsonWrite(project, "updates_per_second"      , mSettings.updates_per_second);
    JsonWrite(project, "working_folder"          , mSettings.working_folder);
    JsonWrite(project, "command_line_arguments"  , mSettings.command_line_arguments);
    JsonWrite(project, "use_gamehost_process"    , mSettings.use_gamehost_process);
    JsonWrite(project, "num_velocity_iterations" , mSettings.num_velocity_iterations);
    JsonWrite(project, "num_position_iterations" , mSettings.num_position_iterations);
    JsonWrite(project, "phys_gravity_x"          , mSettings.gravity.x);
    JsonWrite(project, "phys_gravity_y"          , mSettings.gravity.y);
    JsonWrite(project, "phys_scale_x"            , mSettings.physics_scale.x);
    JsonWrite(project, "phys_scale_y"            , mSettings.physics_scale.y);
    JsonWrite(project, "game_viewport_width"     , mSettings.viewport_width);
    JsonWrite(project, "game_viewport_height"    , mSettings.viewport_height);
    JsonWrite(project, "clear_color"             , mSettings.clear_color);

    // serialize the workspace properties into JSON
    json["workspace"] = QJsonObject::fromVariantMap(mProperties);
    json["project"]   = project;

    // serialize the properties stored in each and every
    // resource object.
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveProperties(json);
    }
    // set the root object to the json document then serialize
    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();

    INFO("Saved workspace data in '%1'", filename);
    return true;
}

void Workspace::SaveUserSettings(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        ERROR("Failed to open file: '%1' for writing. (%2)", filename, file.error());
        return;
    }
    QJsonObject json;
    json["user"] = QJsonObject::fromVariantMap(mUserProperties);
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive())
            continue;
        resource->SaveUserProperties(json);
    }

    QJsonDocument docu(json);
    file.write(docu.toJson());
    file.close();
    INFO("Saved private workspace data in '%1'", filename);
}

bool Workspace::LoadProperties(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ERROR("Failed to open file: '%1'", filename);
        return false;
    }

    const auto& buff = file.readAll(); // QByteArray

    QJsonDocument docu(QJsonDocument::fromJson(buff));

    const QJsonObject& project = docu["project"].toObject();
    JsonReadSafe(project, "multisample_sample_count", &mSettings.multisample_sample_count);
    JsonReadSafe(project, "application_name",         &mSettings.application_name);
    JsonReadSafe(project, "application_version",      &mSettings.application_version);
    JsonReadSafe(project, "application_library_win",  &mSettings.application_library_win);
    JsonReadSafe(project, "application_library_lin",  &mSettings.application_library_lin);
    JsonReadSafe(project, "default_min_filter",       &mSettings.default_min_filter);
    JsonReadSafe(project, "default_mag_filter",       &mSettings.default_mag_filter);
    JsonReadSafe(project, "window_mode",              &mSettings.window_mode);
    JsonReadSafe(project, "window_width",             &mSettings.window_width);
    JsonReadSafe(project, "window_height",            &mSettings.window_height);
    JsonReadSafe(project, "window_can_resize",        &mSettings.window_can_resize);
    JsonReadSafe(project, "window_has_border",        &mSettings.window_has_border);
    JsonReadSafe(project, "window_vsync",             &mSettings.window_vsync);
    JsonReadSafe(project, "window_cursor",            &mSettings.window_cursor);
    JsonReadSafe(project, "config_srgb",              &mSettings.config_srgb);
    JsonReadSafe(project, "ticks_per_second",         &mSettings.ticks_per_second);
    JsonReadSafe(project, "updates_per_second",       &mSettings.updates_per_second);
    JsonReadSafe(project, "working_folder",           &mSettings.working_folder);
    JsonReadSafe(project, "command_line_arguments",   &mSettings.command_line_arguments);
    JsonReadSafe(project, "use_gamehost_process",     &mSettings.use_gamehost_process);
    JsonReadSafe(project, "num_position_iterations",  &mSettings.num_position_iterations);
    JsonReadSafe(project, "num_velocity_iterations",  &mSettings.num_velocity_iterations);
    JsonReadSafe(project, "phys_gravity_x",           &mSettings.gravity.x);
    JsonReadSafe(project, "phys_gravity_y",           &mSettings.gravity.y);
    JsonReadSafe(project, "phys_scale_x",             &mSettings.physics_scale.x);
    JsonReadSafe(project, "phys_scale_y",             &mSettings.physics_scale.y);
    JsonReadSafe(project, "game_viewport_width",      &mSettings.viewport_width);
    JsonReadSafe(project, "game_viewport_height",     &mSettings.viewport_height);
    JsonReadSafe(project, "clear_color",              &mSettings.clear_color);

    // load the workspace properties.
    mProperties = docu["workspace"].toObject().toVariantMap();

    // so we expect that the content has been loaded first.
    // and then ask each resource object to load its additional
    // properties from the workspace file.
    for (auto& resource : mResources)
    {
        resource->LoadProperties(docu.object());
    }

    INFO("Loaded workspace file '%1'", filename);
    return true;
}

void Workspace::LoadUserSettings(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        WARN("Failed to open: '%1' (%2)", filename, file.error());
        return;
    }
    const auto& buff = file.readAll(); // QByteArray
    const QJsonDocument docu(QJsonDocument::fromJson(buff));
    mUserProperties = docu["user"].toObject().toVariantMap();

    for (auto& resource : mResources)
    {
        resource->LoadUserProperties(docu.object());
    }

    INFO("Loaded private workspace data: '%1'", filename);
}

Workspace::ResourceList Workspace::ListAllMaterials() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveMaterials());
    base::AppendVector(list, ListUserDefinedMaterials());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveMaterials() const
{
    return ListResources(Resource::Type::Material, true, true);
}

Workspace::ResourceList Workspace::ListUserDefinedMaterials() const
{
    return ListResources(Resource::Type::Material, false, true);
}

Workspace::ResourceList Workspace::ListAllDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListPrimitiveDrawables());
    base::AppendVector(list, ListUserDefinedDrawables());
    return list;
}

Workspace::ResourceList Workspace::ListPrimitiveDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, true, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, true, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, true, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedDrawables() const
{
    ResourceList list;
    base::AppendVector(list, ListResources(Resource::Type::Drawable, false, false));
    base::AppendVector(list, ListResources(Resource::Type::ParticleSystem, false, false));
    base::AppendVector(list, ListResources(Resource::Type::Shape, false, false));

    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return list;
}

Workspace::ResourceList Workspace::ListUserDefinedEntities() const
{
    return ListResources(Resource::Type::Entity, false, true);
}

QStringList Workspace::ListUserDefinedEntityIds() const
{
    QStringList list;
    for (const auto& resource : mResources)
    {
        if (!resource->IsEntity())
            continue;
        list.append(resource->GetId());
    }
    return list;
}

Workspace::ResourceList Workspace::ListResources(Resource::Type type, bool primitive, bool sort) const
{
    ResourceList list;
    for (const auto& resource : mResources)
    {
        if (resource->IsPrimitive() == primitive &&
            resource->GetType() == type)
        {
            ListItem item;
            item.name = resource->GetName();
            item.id   = resource->GetId();
            list.push_back(item);
        }
    }
    if (sort)
    {
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    }
    return list;
}

void Workspace::SaveResource(const Resource& resource)
{
    RECURSION_GUARD(this, "ResourceList");

    const auto& id = resource.GetId();
    for (size_t i=0; i<mResources.size(); ++i)
    {
        auto& res = mResources[i];
        if (res->GetId() != id)
            continue;
        res->UpdateFrom(resource);
        emit ResourceUpdated(mResources[i].get());
        emit dataChanged(index(i, 0), index(i, 1));
        return;
    }
    // if we're here no such resource exists yet.
    // Create a new resource and add it to the list of resources.
    beginInsertRows(QModelIndex(), mVisibleCount, mVisibleCount);
    // insert at the end of the visible range which is from [0, mVisibleCount)
    mResources.insert(mResources.begin() + mVisibleCount, resource.Copy());

    // careful! endInsertRows will trigger the view proxy to refetch the contents.
    // make sure to update this property before endInsertRows or otherwise
    // we'll hit an assert incorrectly in GetUserDefinedProperty.
    mVisibleCount++;

    endInsertRows();

    auto& back = mResources[mVisibleCount];
    emit NewResourceAvailable(back.get());

}

QString Workspace::MapDrawableIdToName(const QString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapMaterialIdToName(const QString& id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapEntityIdToName(const QString &id) const
{
    return MapResourceIdToName(id);
}

QString Workspace::MapResourceIdToName(const QString &id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id)
            return resource->GetName();
    }
    return "";
}

bool Workspace::IsValidMaterial(const QString& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetType() == Resource::Type::Material &&
            resource->GetId() == klass)
            return true;
    }
    return false;
}

bool Workspace::IsValidDrawable(const QString& klass) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == klass &&
            (resource->GetType() == Resource::Type::ParticleSystem ||
             resource->GetType() == Resource::Type::Shape ||
             resource->GetType() == Resource::Type::Drawable))
            return true;
    }
    return false;
}

bool Workspace::IsValidScript(const QString& id) const
{
    for (const auto& resource : mResources)
    {
        if (resource->GetId() == id &&
            resource->IsScript())
            return true;
    }
    return false;
}

Resource& Workspace::GetResource(size_t index)
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}
Resource& Workspace::GetPrimitiveResource(size_t index)
{
    const auto num_primitives = mResources.size() - mVisibleCount;

    ASSERT(index < num_primitives);
    return *mResources[mVisibleCount + index];
}

Resource& Workspace::GetUserDefinedResource(size_t index)
{
    ASSERT(index < mVisibleCount);

    return *mResources[index];
}

Resource* Workspace::FindResourceById(const QString &id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}

Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type)
{
    for (auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}
Resource& Workspace::GetResourceById(const QString& id)
{
    for (auto& res : mResources)
    {
        if (res->GetId() == id)
            return *res;
    }
    BUG("No such resource.");
}


const Resource& Workspace::GetResourceByName(const QString& name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetType() == type && res->GetName() == name)
            return *res;
    }
    BUG("No such resource");
}

const Resource* Workspace::FindResourceById(const QString &id) const
{
    for (const auto& res : mResources)
    {
        if (res->GetId() == id)
            return res.get();
    }
    return nullptr;
}
const Resource* Workspace::FindResourceByName(const QString &name, Resource::Type type) const
{
    for (const auto& res : mResources)
    {
        if (res->GetName() == name && res->GetType() == type)
            return res.get();
    }
    return nullptr;
}

const Resource& Workspace::GetResource(size_t index) const
{
    ASSERT(index < mResources.size());
    return *mResources[index];
}

const Resource& Workspace::GetUserDefinedResource(size_t index) const
{
    ASSERT(index < mVisibleCount);
    return *mResources[index];
}
const Resource& Workspace::GetPrimitiveResource(size_t index) const
{
    const auto num_primitives = mResources.size() - mVisibleCount;

    ASSERT(index < num_primitives);
    return *mResources[mVisibleCount + index];
}

void Workspace::DeleteResources(const QModelIndexList& list)
{
    std::vector<size_t> indices;
    for (const auto& i : list)
        indices.push_back(i.row());

    DeleteResources(indices);
}
void Workspace::DeleteResource(size_t index)
{
    DeleteResources(std::vector<size_t>{index});
}

void Workspace::DeleteResources(std::vector<size_t> indices)
{
    RECURSION_GUARD(this, "ResourceList");

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    // because the high probability of unwanted recursion
    // messing this iteration up (for example by something
    // calling back to this workspace from Resource
    // deletion signal handler and adding a new resource) we
    // must take some special care here.
    // So therefore first put the resources to be deleted into
    // a separate container while iterating and removing from the
    // removing from the primary list and only then invoke the signal
    // for each resource.
    std::vector<std::unique_ptr<Resource>> graveyard;

    for (int i=0; i<indices.size(); ++i)
    {
        const auto row = indices[i] - i;
        beginRemoveRows(QModelIndex(), row, row);

        auto it = std::begin(mResources);
        std::advance(it, row);
        graveyard.push_back(std::move(*it));
        mResources.erase(it);
        mVisibleCount--;

        endRemoveRows();
    }
    // invoke a resource deletion signal for each resource now
    // by iterating over the separate container. (avoids broken iteration)
    for (const auto& carcass : graveyard)
    {
        emit ResourceToBeDeleted(carcass.get());
    }

    // script resources are special in the sense that they're the only
    // resources where the underlying file system content file is actually
    // created by this editor. for everything else, shaders, image files
    // and font files the resources are created by other tools/applications
    // and we only keep references to those files.
    // So for scripts when the script resource is deleted we're actually
    // going to delete the underlying filesystem file as well.
    for (const auto& carcass : graveyard)
    {
        if (!carcass->IsScript())
            continue;
        Script* script = nullptr;
        carcass->GetContent(&script);
        const auto& file = MapFileToFilesystem(script->GetFileURI());
        if (!QFile::remove(file))
        {
            ERROR("Failed to remove file '%1'", file);
        }
        else
        {
            INFO("Deleted '%1'", file);
        }
    }
}

void Workspace::DuplicateResources(const QModelIndexList& list)
{
    std::vector<size_t> indices;
    for (const auto& i : list)
        indices.push_back(i.row());
    DuplicateResources(indices);
}

void Workspace::DuplicateResources(std::vector<size_t> indices)
{
    RECURSION_GUARD(this, "ResourceList");

    std::sort(indices.begin(), indices.end(), std::less<size_t>());

    std::vector<std::unique_ptr<Resource>> dupes;
    for (int i=0; i<indices.size(); ++i)
    {
        const auto row = indices[i];
        const auto& resource = GetResource(row);
        auto clone = resource.Clone();
        clone->SetName(QString("Copy of %1").arg(resource.GetName()));
        dupes.push_back(std::move(clone));
    }

    for (int i=0; i<(int)dupes.size(); ++i)
    {
        const auto pos = indices[i]+ i;
        beginInsertRows(QModelIndex(), pos, pos);
        auto it = mResources.begin();
        it += pos;
        auto* dupe = dupes[i].get();
        mResources.insert(it, std::move(dupes[i]));
        mVisibleCount++;
        endInsertRows();

        emit NewResourceAvailable(dupe);
    }
}

void Workspace::DuplicateResource(size_t index)
{
    DuplicateResources(std::vector<size_t>{index});
}

void Workspace::ImportFilesAsResource(const QStringList& files)
{
    // todo: given a collection of file names some of the files
    // could belong together in a sprite/texture animation sequence.
    // for example if we have "bird_0.png", "bird_1.png", ... "bird_N.png" 
    // we could assume that these are all material animation frames
    // and should go together into one material.
    // On the other hand there are also cases like with tile sets that have
    // tiles named tile1.png, tile2.png ... and these should be separated.
    // not sure how to deal with this smartly. 

    for (const QString& file : files)
    {
        const QFileInfo info(file);
        if (!info.isFile())
        {
            WARN("File '%1' is not actually a file.", file);
            continue;
        }
        const auto& name   = info.baseName();
        const auto& suffix = info.completeSuffix().toUpper();
        if (suffix == "LUA")
        {
            const auto& uri = MapFileToWorkspace(file);
            Script script;
            script.SetFileURI(ToUtf8(uri));
            ScriptResource res(script, name);
            SaveResource(res);
            INFO("Imported new script file '%1' based on file '%2'", name, info.filePath());
        }
        else if (suffix == "JPEG" || suffix == "JPG" || suffix == "PNG" || suffix == "TGA" || suffix == "BMP")
        {
            const auto& uri = MapFileToWorkspace(file);
            gfx::detail::TextureFileSource texture;
            texture.SetFileName(ToUtf8(uri));
            texture.SetName(ToUtf8(name));

            gfx::TextureMap2DClass klass;
            klass.SetSurfaceType(gfx::MaterialClass::SurfaceType::Transparent);
            klass.SetTexture(texture.Copy());
            klass.SetTextureMinFilter(gfx::MaterialClass::MinTextureFilter::Default);
            klass.SetTextureMagFilter(gfx::MaterialClass::MagTextureFilter ::Default);
            MaterialResource res(klass, name);
            SaveResource(res);
            INFO("Imported new material '%1' based on image file '%2'", name, info.filePath());
        }
        else if (suffix == "MP3" || suffix == "WAV" || suffix == "FLAC" || suffix == "OGG")
        {
            const auto& uri = MapFileToWorkspace(file);
            AudioFile audio;
            audio.SetFileURI(ToUtf8(uri));
            AudioResource res(audio, name);
            SaveResource(res);
            INFO("Imported new audio file '%1' based on file '%2'", name, info.filePath());
        }
        else
        {
            const auto& uri = MapFileToWorkspace(file);
            DataFile data;
            data.SetFileURI(ToUtf8(uri));
            DataResource res(data, name);
            SaveResource(res);
            INFO("Imported new data file '%1' based on file '%2'", name, info.filePath());
        }
    }
}

void Workspace::Tick()
{

}

bool Workspace::PackContent(const std::vector<const Resource*>& resources, const ContentPackingOptions& options)
{
    const QString& outdir = JoinPath(options.directory, options.package_name);
    if (!MakePath(outdir))
    {
        ERROR("Failed to create %1", outdir);
        return false;
    }

    // unfortunately we need to make copies of the resources
    // since packaging might modify the resources yet the
    // original resources should not be changed.
    // todo: perhaps rethink this.. what other ways would there be ?
    // constraints:
    //  - don't wan to duplicate the serialization/deserialization/JSON writing
    //  - should not know details of resources (materials, drawables etc)
    //  - material depends on resource packer, resource packer should not then
    //    know about material
    std::vector<std::unique_ptr<Resource>> mutable_copies;
    for (const auto* resource : resources)
    {
        mutable_copies.push_back(resource->Copy());
    }

    ResourcePacker packer(outdir,
        options.max_texture_width,
        options.max_texture_height,
        options.texture_padding,
        options.resize_textures,
        options.combine_textures);

    // collect the resources in the packer.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Collecting resources...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to Resource interface ?
            const gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->BeginPacking(&packer);
        }
        else if (resource->IsParticleEngine())
        {
            const gfx::KinematicsParticleEngineClass* engine = nullptr;
            resource->GetContent(&engine);
            engine->Pack(&packer);
        }
        else if (resource->IsCustomShape())
        {
            const gfx::PolygonClass* polygon = nullptr;
            resource->GetContent(&polygon);
            polygon->Pack(&packer);
        }
    }

    unsigned errors = 0;

    // copy some file based content around.
    // todo: this would also need some kind of file name collision
    // resolution and mapping functionality.
    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        if (resource->IsScript())
        {
            const Script* script = nullptr;
            resource->GetContent(&script);
            packer.CopyFile(script->GetFileURI(), "lua/");
        }
        else if (resource->IsDataFile())
        {
            const DataFile* datafile = nullptr;
            resource->GetContent(&datafile);
            packer.CopyFile(datafile->GetFileURI(), "data/");
        }
        else if (resource->IsAudioFile())
        {
            const AudioFile* audio = nullptr;
            resource->GetContent(&audio);
            packer.CopyFile(audio->GetFileURI(), "audio/");
        }
        else if (resource->IsUI())
        {
            uik::Window* window = nullptr;
            resource->GetContent(&window);
            // package the style resources. currently this is only the font files.
            auto style_data = LoadGameData(window->GetStyleName());
            if (!style_data)
            {
                ERROR("Failed to open '%1'", window->GetStyleName());
                errors++;
                continue;
            }
            game::UIStyle style;
            if (!style.LoadStyle(*style_data))
            {
                ERROR("Failed to load UI style '%1'", window->GetStyleName());
                errors++;
                continue;
            }
            std::vector<game::UIStyle::PropertyKeyValue> props;
            style.GatherProperties("font-name", &props);
            for (auto& p : props)
            {
                std::string src_font_uri;
                std::string dst_font_uri;
                p.prop.GetValue(&src_font_uri);
                dst_font_uri = packer.CopyFile(src_font_uri, "fonts/");
                p.prop.SetValue(dst_font_uri);
                style.SetProperty(p.key, p.prop);
            }
            window->SetStyleName(packer.CopyFile(window->GetStyleName(), "ui/"));
            // for each widget, parse the style string and see if there are more font-name props.
            window->ForEachWidget([&style, &packer](uik::Widget* widget) {
                auto style_string = widget->GetStyleString();
                if (style_string.empty())
                    return;
                DEBUG("Widget '%1' style string '%2'", widget->GetId(), style_string);
                style.ClearProperties();
                style.ClearMaterials();
                style.ParseStyleString(widget->GetId(), style_string);
                std::vector<game::UIStyle::PropertyKeyValue> props;
                style.GatherProperties("font-name", &props);
                for (auto& p : props)
                {
                    std::string src_font_uri;
                    std::string dst_font_uri;
                    p.prop.GetValue(&src_font_uri);
                    dst_font_uri = packer.CopyFile(src_font_uri, "fonts/");
                    p.prop.SetValue(dst_font_uri);
                    style.SetProperty(p.key, p.prop);
                }
                style_string = style.MakeStyleString(widget->GetId());
                // this is a bit of a hack but we know that the style string
                // contains the widget id for each property. removing the
                // widget id from the style properties:
                // a) saves some space
                // b) makes the style string copyable from one widget to another as-s
                boost::erase_all(style_string, widget->GetId() + "/");
                DEBUG("Reset widget '%1' style string '%2'", widget->GetId(), style_string);
                widget->SetStyleString(std::move(style_string));
            });
        }
        else if (resource->IsEntity())
        {
            game::EntityClass* entity = nullptr;
            resource->GetContent(&entity);
            for (size_t i=0; i<entity->GetNumNodes(); ++i)
            {
                auto& node = entity->GetNode(i);
                if (!node.HasTextItem())
                    continue;
                auto* text = node.GetTextItem();
                text->SetFontName(packer.CopyFile(text->GetFontName(), "fonts/"));
            }
        }
    }

    packer.PackTextures([this](const std::string& action, int step, int max) {
        emit ResourcePackingUpdate(FromLatin(action), step, max);
    });

    for (int i=0; i<mutable_copies.size(); ++i)
    {
        const auto& resource = mutable_copies[i];
        emit ResourcePackingUpdate("Updating resources references...", i, mutable_copies.size());
        if (resource->IsMaterial())
        {
            // todo: maybe move to resource interface ?
            gfx::MaterialClass* material = nullptr;
            resource->GetContent(&material);
            material->FinishPacking(&packer);
        }
    }

    // write content file ?
    if (options.write_content_file)
    {
        emit ResourcePackingUpdate("Writing content JSON file...", 0, 0);
        // filename of the JSON based descriptor that contains all the
        // resource definitions.
        const auto &json_filename = JoinPath(outdir, "content.json");

        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open file: '%1' for writing (%2)", json_filename, json_file.error());
            return false;
        }

        // finally serialize
        data::JsonObject json;
        json.Write("json_version", 1);
        json.Write("made_with_app", APP_TITLE);
        json.Write("made_with_ver", APP_VERSION);
        for (const auto &resource : mutable_copies)
        {
            resource->Serialize(json);
        }

        const auto &str = json.ToString();
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write JSON file: '%1' %2", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    // write config file?
    if (options.write_config_file)
    {
        emit ResourcePackingUpdate("Writing config JSON file...", 0, 0);

        const auto& json_filename = JoinPath(outdir, "config.json");
        QFile json_file;
        json_file.setFileName(json_filename);
        json_file.open(QIODevice::WriteOnly);
        if (!json_file.isOpen())
        {
            ERROR("Failed to open file: '%1' for writing (%2)", json_filename, json_file.error());
            return false;
        }
        nlohmann:: json json;
        base::JsonWrite(json, "json_version",   1);
        base::JsonWrite(json, "made_with_app",  APP_TITLE);
        base::JsonWrite(json, "made_with_ver",  APP_VERSION);
        base::JsonWrite(json["config"], "red_size",     8);
        base::JsonWrite(json["config"], "green_size",   8);
        base::JsonWrite(json["config"], "blue_size",    8);
        base::JsonWrite(json["config"], "alpha_size",   8);
        base::JsonWrite(json["config"], "stencil_size", 8);
        base::JsonWrite(json["config"], "depth_size",   0);
        base::JsonWrite(json["config"], "srgb", mSettings.config_srgb);
        if (mSettings.multisample_sample_count == 0)
            base::JsonWrite(json["config"], "sampling", "None");
        else if (mSettings.multisample_sample_count == 4)
            base::JsonWrite(json["config"], "sampling", "MSAA4");
        else if (mSettings.multisample_sample_count == 8)
            base::JsonWrite(json["config"], "sampling", "MSAA8");
        else if (mSettings.multisample_sample_count == 16)
            base::JsonWrite(json["config"], "sampling", "MSAA16");
        base::JsonWrite(json["window"], "width",      mSettings.window_width);
        base::JsonWrite(json["window"], "height",     mSettings.window_height);
        base::JsonWrite(json["window"], "can_resize", mSettings.window_can_resize);
        base::JsonWrite(json["window"], "has_border", mSettings.window_has_border);
        base::JsonWrite(json["window"], "vsync",      mSettings.window_vsync);
        base::JsonWrite(json["window"], "cursor",     mSettings.window_cursor);
        if (mSettings.window_mode == ProjectSettings::WindowMode::Windowed)
            base::JsonWrite(json["window"], "set_fullscreen", false);
        else if (mSettings.window_mode == ProjectSettings::WindowMode::Fullscreen)
            base::JsonWrite(json["window"], "set_fullscreen", true);


        base::JsonWrite(json["application"], "title",    ToUtf8(mSettings.application_name));
        base::JsonWrite(json["application"], "version",  ToUtf8(mSettings.application_version));
        base::JsonWrite(json["application"], "ticks_per_second",   (float)mSettings.ticks_per_second);
        base::JsonWrite(json["application"], "updates_per_second", (float)mSettings.updates_per_second);
        base::JsonWrite(json["application"], "content", "content.json");
        base::JsonWrite(json["application"], "default_min_filter", mSettings.default_min_filter);
        base::JsonWrite(json["application"], "default_mag_filter", mSettings.default_mag_filter);
        base::JsonWrite(json["physics"], "num_velocity_iterations", mSettings.num_velocity_iterations);
        base::JsonWrite(json["physics"], "num_position_iterations", mSettings.num_position_iterations);
        base::JsonWrite(json["physics"], "gravity", mSettings.gravity);
        base::JsonWrite(json["physics"], "scale",   mSettings.physics_scale);
        base::JsonWrite(json["engine"], "clear_color", ToGfx(mSettings.clear_color));

        // resolves the path.
        const QFileInfo engine_dll(mSettings.GetApplicationLibrary());
        QString engine_name = engine_dll.fileName();
        if (engine_name.startsWith("lib"))
            engine_name.remove(0, 3);
        if (engine_name.endsWith(".so"))
            engine_name.chop(3);
        else if (engine_name.endsWith(".dll"))
            engine_name.chop(4);
        json["application"]["library"] = ToUtf8(engine_name);

        const auto& str = json.dump(2);
        if (json_file.write(&str[0], str.size()) == -1)
        {
            ERROR("Failed to write JSON file: '%1' '%2'", json_filename, json_file.error());
            return false;
        }
        json_file.flush();
        json_file.close();
    }

    const auto total_errors = errors + packer.GetNumErrors();
    if (total_errors)
    {
        WARN("Resource packing completed with errors (%1).", total_errors);
        WARN("Please see the log file for details.");
        return false;
    }
    // Copy game main executable.
    std::string runner = "GameMain";
#if defined(WINDOWS_OS)
    runner.append(".exe");
#endif
    packer.CopyFile(runner, "");
    // copy the engine dll.
    packer.CopyFile(ToUtf8(mSettings.GetApplicationLibrary()), "");

    INFO("Packed %1 resource(s) into '%2' successfully.", resources.size(), outdir);
    return true;
}

void Workspace::UpdateResource(const Resource* resource)
{
    SaveResource(*resource);
}

void Workspace::UpdateUserProperty(const QString& name, const QVariant& data)
{
    mUserProperties[name] = data;
}

} // namespace

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

#include <unordered_map>
#include <memory>

#include "gamelib/asset.h"
#include "gamelib/gfxfactory.h"
#include "graphics/resource.h"
#include "gfxfactory.h"

namespace game
{
    class Animation;

    // Game content (assets + gfx resources) loader.
    // Provides access to high level game content, i.e. game assets such as
    // animations based on their descriptions in the JSON file(s).
    // Additionally implements the gfx::ResourceLoader in order to implement
    // access to the low level file based graphics resources such as
    // texture, font and shader files.
    class ContentLoader : public GfxFactory,
                          public AssetTable,
                          public gfx::ResourceLoader
    {
    public:
        // GfxFactory implementation. Provides low level GFX
        // resource creation for the game level subsystem.
        virtual std::shared_ptr<gfx::Material> MakeMaterial(const std::string& name) const override;
        virtual std::shared_ptr<gfx::Drawable> MakeDrawable(const std::string& name) const override;

        // AssetTable implementation.
        virtual const Animation* FindAnimation(const std::string& name) const override;
        virtual Animation* FindAnimation(const std::string& name) override;
        // Read the given resource description file.
        // The expectation is that the file is well formed.
        // Throws an exception on error ill-formed file.
        // However no validation is done regarding the completeness
        // of the content and resources that are loaded from the file.
        virtual void LoadFromFile(const std::string& dir, const std::string& file) override;

        // gfx::ResourceLoader implementation. Provides access to the
        // low level byte buffer / file system file resources such as
        // textures/font files, shaders used by the graphics subsystem.
        virtual std::string ResolveFile(gfx::ResourceLoader::ResourceType type,
            const std::string& file) const override;

    private:
        std::string mResourceDir;
        std::string mResourceFile;
        // These are the material objects that have been loaded
        // from the resource file. These instances can be used
        // directly when a global material instance is wanted.
        // in case of a unique instance then a copy needs to be made.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::Material>> mGlobalMaterialInstances;
        // Similar to the materials these are the global material
        // instances that have been loaded from the resource file.
        // These can be used directly when a global instance is needed
        // otherwise in case of a unique instance a copy needs to be made.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::Drawable>> mGlobalDrawableInstances;

        std::unordered_map<std::string,
            std::shared_ptr<Animation>> mAnimations;

    };
} // namespace

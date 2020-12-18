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

#include "gamelib/classlib.h"
#include "graphics/resource.h"
#include "graphics/material.h"
#include "graphics/drawable.h"

namespace game
{
    class Animation;
    class AnimationClass;
    class AnimationNode;
    class AnimationNodeClass;
    class SceneClass;

    // Game content (assets + gfx resources) loader.
    // Provides access to high level game content, i.e. game assets such as
    // animations based on their descriptions in the JSON file(s).
    // Additionally implements the gfx::ResourceLoader in order to implement
    // access to the low level file based graphics resources such as
    // texture, font and shader files.
    class ContentLoader : public ClassLibrary,
                          public gfx::ResourceLoader
    {
    public:
        // ClassLibrary implementation.
        // resource creation for the game level subsystem.
        virtual std::shared_ptr<const gfx::MaterialClass> FindMaterialClass(const std::string& name) const override;
        virtual std::shared_ptr<const gfx::DrawableClass> FindDrawableClass(const std::string& name) const override;
        virtual std::shared_ptr<const AnimationClass> FindAnimationClassByName(const std::string& name) const override;
        virtual std::shared_ptr<const AnimationClass> FindAnimationClassById(const std::string& id) const override;
        virtual std::shared_ptr<const game::SceneClass> FindSceneClassByName(const std::string& name) const override;
        virtual std::shared_ptr<const game::SceneClass> FindSceneClassById(const std::string& id) const override;
        virtual void LoadFromFile(const std::string& dir, const std::string& file) override;
        // gfx::ResourceLoader implementation. Provides access to the
        // low level byte buffer / file system file resources such as
        // textures/font files, shaders used by the graphics subsystem.
        virtual std::string ResolveURI(gfx::ResourceLoader::ResourceType type, const std::string& URI) const override;

    private:
        std::string mResourceDir;
        std::string mResourceFile;
        // These are the material types that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::MaterialClass>> mMaterials;
        // These are the particle engine types that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::KinematicsParticleEngineClass>> mParticleEngines;
        // These are the custom shapes (polygons) that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<gfx::PolygonClass>> mCustomShapes;
        // These are the animations that have been loaded
        // from the resource file.
        std::unordered_map<std::string,
            std::shared_ptr<AnimationClass>> mAnimations;
        // These are the scenes that have been loaded from
        // the resource file.
        std::unordered_map<std::string, std::shared_ptr<SceneClass>> mScenes;

        // name table. maps animation names to ids.
        std::unordered_map<std::string, std::string> mAnimationNameTable;
        // name table maps scene names to ids.
        std::unordered_map<std::string, std::string> mSceneNameTable;

        // cache of URIs that have been resolved to file
        // names already.
        mutable std::unordered_map<std::string, std::string> mUriCache;

    };
} // namespace

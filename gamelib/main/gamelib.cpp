// Copyright (c) 2010-2014 Sami Väisänen, Ensisoft
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

#define GAMESTUDIO_GAMELIB_IMPLEMENTATION

#include "base/assert.h"
#include "base/logging.h"
#include "gamelib/main/interface.h"
#include "gamelib/loader.h"
#include "graphics/resource.h"

// helper stuff for dependency management for now.
// see interface.h for more details.

extern "C" {
GAMESTUDIO_EXPORT void CreateDefaultEnvironment(game::ClassLibrary** gfx_factory,
    game::AssetTable** asset_table)
{
    auto* ret = new game::ContentLoader();
    *gfx_factory = ret;
    *asset_table = ret;
    gfx::SetResourceLoader(ret);
}
GAMESTUDIO_EXPORT void DestroyDefaultEnvironment(game::ClassLibrary* gfx_factory,
    game::AssetTable* asset_table)
{
    gfx::SetResourceLoader(nullptr);
    ASSERT(dynamic_cast<game::ContentLoader*>(gfx_factory) ==
           dynamic_cast<game::ContentLoader*>(asset_table));
    delete asset_table;
}

GAMESTUDIO_EXPORT void SetResourceLoader(gfx::ResourceLoader* loader)
{
    gfx::SetResourceLoader(loader);
}

GAMESTUDIO_EXPORT void SetGlobalLogger(base::Logger* logger)
{
    base::SetGlobalLog(logger);
}

} // extern "C"

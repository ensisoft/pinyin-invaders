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

#pragma once

#include "config.h"

#include <fstream>
#include <string>
#include <stdexcept>

#include "base/assert.h"
#include "third_party/stb/stb_image.h"
#include "bitmap.h"

namespace gfx
{
    // Load images into CPU memory
    class Image
    {
    public: 
        Image() = default;
        Image(const std::string& file)
        {
            Load(file);
        }
        Image(const Image&) = delete;
       ~Image() 
        { 
            if (mData)
                stbi_image_free(mData);
        }

        void Load(const std::string& filename)
        {
            std::ifstream in(filename, std::ios::in | std::ios::binary);
            if (!in)
                throw std::runtime_error("failed to open file: " + filename);

            in.seekg(0, std::ios::end);
            const auto size = (std::size_t)in.tellg();
            in.seekg(0, std::ios::beg);

            std::vector<char> data;
            data.resize(size);
            in.read(&data[0], size);
            if ((std::size_t)in.gcount() != size)
                throw std::runtime_error("failed to read all of: " + filename);

            int xres  = 0;
            int yres  = 0;
            int depth = 0;
            auto* bmp = stbi_load_from_memory((const stbi_uc*)data.data(),
                (int)data.size(), &xres, &yres, &depth, 0);
            if (bmp == nullptr)
                throw std::runtime_error("failed to decompress texture: " + filename);
            if (mData)
                stbi_image_free(mData);

            mFilename = filename;
            mWidth  = static_cast<unsigned>(xres);
            mHeight = static_cast<unsigned>(yres);
            mDepth  = static_cast<unsigned>(depth);
            mData   = bmp;
        }

        template<typename PixelT>
        Bitmap<PixelT> AsBitmap() const 
        { 
            ASSERT(mData);
            if (mDepth == sizeof(PixelT))
                return Bitmap<PixelT>(reinterpret_cast<const PixelT*>(mData), mWidth, mHeight);
                    
            const Rect rc(0, 0, mWidth, mHeight);

            Bitmap<PixelT> ret(mWidth, mHeight);        
            if (mDepth == 1)
                ret.Copy(rc, reinterpret_cast<const Grayscale*>(mData));
            else if (mDepth == 3)
                ret.Copy(rc, reinterpret_cast<const RGB*>(mData));
            else if (mDepth == 4)
                ret.Copy(rc, reinterpret_cast<const RGBA*>(mData));
            
            return ret;
        }
        bool IsValid() const 
        { return mData != nullptr; }
        unsigned GetWidth() const 
        { return mWidth; }
        unsigned GetHeight() const 
        { return mHeight; }
        unsigned GetDepthBits() const 
        { return mDepth * 8; }
        const void* GetData() const 
        { return mData; }

        Image& operator=(const Image&) = delete;
    private:
        std::string mFilename;
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        unsigned mDepth  = 0;
        stbi_uc* mData   = nullptr;
    };
} // namespace
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

#include "config.h"

#include <GLES2/gl2.h>

#include <cstdio>
#include <cassert>
#include <cstring> // for memcpy
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>

#include "base/assert.h"
#include "base/logging.h"
#include "shader.h"
#include "program.h"
#include "device.h"
#include "geometry.h"
#include "texture.h"
#include "color4f.h"

#define GL_CALL(x) \
    do {                                                                        \
        mGL.x;                                                           \
        {                                                                       \
            const auto err = mGL.glGetError();                           \
            if (err != GL_NO_ERROR) {                                           \
                std::printf("GL Error 0x%04x '%s' @ %s,%d\n", err, GLEnumToStr(err),  \
                    __FILE__, __LINE__);                                        \
                std::abort();                                                   \
            }                                                                   \
        }                                                                       \
    } while(0)


namespace
{

const char* GLEnumToStr(GLenum eval)
{
#define CASE(x) case x: return #x
    switch (eval)
    {
        CASE(GL_NO_ERROR);
        CASE(GL_INVALID_ENUM);
        CASE(GL_INVALID_VALUE);
        CASE(GL_INVALID_OPERATION);
        CASE(GL_OUT_OF_MEMORY);
        CASE(GL_STATIC_DRAW);
        CASE(GL_STREAM_DRAW);
        CASE(GL_ELEMENT_ARRAY_BUFFER);
        CASE(GL_ARRAY_BUFFER);

        // framebuffer
        CASE(GL_FRAMEBUFFER_COMPLETE);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
        CASE(GL_FRAMEBUFFER_UNSUPPORTED);

        CASE(GL_FRAGMENT_SHADER);
        CASE(GL_VERTEX_SHADER);
    }
    return "???";
#undef CASE
}

template<typename To, typename From>
const To* SafeCast(const From* from)
{
    const auto* ptr = dynamic_cast<const To*>(from);
    ASSERT(ptr);
    return ptr;
}

} // namespace

namespace gfx
{

// This struct holds the OpenGL ES 2.0 entry points that 
// we need in this device implementation. 
// 
// Few notes about this particular implementation:
//
// 1. The pointers are members of an object instead of global
// function pointers because in fact it's possible that the
// pointers would change between one context to another context
// depending on the particular configuration used to create the
// context. (For example GDI pixel format on windows).
// So obviously one set of global function pointers would not then
// work for multiple devices should the function addresses change.
// 
// 2. We're not using something like GLEW here because GLEW has a
// problem in that it doesn't use runtime "get proc" type of function
// resolution for *all* functions. I.e. it leaves the old functions
// (back from the OpenGL 1, fixed pipeline age) unresolved and expects
// them to be exported by the GL library and that the user also then
// links against -lGL.
// This however is incorrect in cases where the OpenGL context is 
// provided by some "virtual context system", e.e libANGLE or Qt's
// QOpenGLWidget.We do not know the underlying OpenGL implementation
// and should not know any such details that GLEW would expect us to
// know. Rather a better way to deal with the problem is to resolve all
// functions in the same manner.
struct OpenGLFunctions 
{
    PFNGLCREATEPROGRAMPROC           glCreateProgram;
    PFNGLCREATESHADERPROC            glCreateShader;
    PFNGLSHADERSOURCEPROC            glShaderSource;
    PFNGLGETERRORPROC                glGetError;
    PFNGLCOMPILESHADERPROC           glCompileShader;
    PFNGLDETACHSHADERPROC            glAttachShader;
    PFNGLDELETESHADERPROC            glDeleteShader;
    PFNGLLINKPROGRAMPROC             glLinkProgram;
    PFNGLUSEPROGRAMPROC              glUseProgram;
    PFNGLVALIDATEPROGRAMPROC         glValidateProgram;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram;
    PFNGLCOLORMASKPROC               glColorMask;
    PFNGLSTENCILFUNCPROC             glStencilFunc;
    PFNGLSTENCILOPPROC               glStencilOp;
    PFNGLCLEARCOLORPROC              glClearColor;
    PFNGLCLEARPROC                   glClear;
    PFNGLCLEARSTENCILPROC            glClearStencil;
    PFNGLCLEARDEPTHFPROC             glClearDepthf;
    PFNGLBLENDFUNCPROC               glBlendFunc;
    PFNGLVIEWPORTPROC                glViewport;
    PFNGLDRAWARRAYSPROC              glDrawArrays;
    PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLGETSTRINGPROC               glGetString;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
    PFNGLUNIFORM1IPROC               glUniform1i;
    PFNGLUNIFORM1FPROC               glUniform1f;
    PFNGLUNIFORM2FPROC               glUniform2f;
    PFNGLUNIFORM3FPROC               glUniform3f;
    PFNGLUNIFORM4FPROC               glUniform4f;
    PFNGLUNIFORM2FVPROC              glUniform2fv;
    PFNGLUNIFORM3FVPROC              glUniform3fv;
    PFNGLUNIFORM4FPROC               glUniform4fv;
    PFNGLUNIFORMMATRIX2FVPROC        glUniformMatrix2fv;
    PFNGLUNIFORMMATRIX3FVPROC        glUniformMatrix3fv;
    PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv;
    PFNGLGETSHADERIVPROC             glGetShaderiv;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog;    
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
    PFNGLDELETETEXTURESPROC          glDeleteTextures;
    PFNGLGENTEXTURESPROC             glGenTextures;
    PFNGLBINDTEXTUREPROC             glBindTexture;
    PFNGLACTIVETEXTUREPROC           glActiveTexture;
    PFNGLGENERATEMIPMAPPROC          glGenerateMipmap;
    PFNGLTEXIMAGE2DPROC              glTexImage2D;
    PFNGLTEXPARAMETERIPROC           glTexParameteri;
    PFNGLPIXELSTOREIPROC             glPixelStorei;
    PFNGLENABLEPROC                  glEnable;
    PFNGLDISABLEPROC                 glDisable;
    PFNGLGETINTEGERVPROC             glGetIntegerv;
};

//
// OpenGL ES 2.0 based custom graphics device implementation
// try to keep this implementantation free of Qt in
// order to promote portability to possibly emscripten
// or Qt free implementation.
class OpenGLES2GraphicsDevice : public Device
{
public:
    OpenGLES2GraphicsDevice(std::shared_ptr<Device::Context> context)
        : mContext(context)
    {
    #define RESOLVE(x) mGL.x = reinterpret_cast<decltype(mGL.x)>(mContext->Resolve(#x));
        RESOLVE(glCreateProgram);
        RESOLVE(glCreateShader);
        RESOLVE(glShaderSource);
        RESOLVE(glGetError);
        RESOLVE(glCompileShader);
        RESOLVE(glAttachShader);
        RESOLVE(glDeleteShader);
        RESOLVE(glLinkProgram);
        RESOLVE(glUseProgram);
        RESOLVE(glValidateProgram);
        RESOLVE(glDeleteProgram);
        RESOLVE(glColorMask);
        RESOLVE(glStencilFunc);
        RESOLVE(glStencilOp);
        RESOLVE(glClearColor);
        RESOLVE(glClear);
        RESOLVE(glClearStencil);
        RESOLVE(glClearDepthf);
        RESOLVE(glBlendFunc);
        RESOLVE(glViewport);
        RESOLVE(glDrawArrays);
        RESOLVE(glGetAttribLocation);
        RESOLVE(glVertexAttribPointer);
        RESOLVE(glEnableVertexAttribArray);
        RESOLVE(glGetString);
        RESOLVE(glGetUniformLocation);
        RESOLVE(glUniform1i);
        RESOLVE(glUniform1f);
        RESOLVE(glUniform2f);
        RESOLVE(glUniform3f);
        RESOLVE(glUniform4f);
        RESOLVE(glUniform2fv);
        RESOLVE(glUniform3fv);
        RESOLVE(glUniform4fv);
        RESOLVE(glUniformMatrix2fv);
        RESOLVE(glUniformMatrix3fv);
        RESOLVE(glUniformMatrix4fv);
        RESOLVE(glGetProgramiv);
        RESOLVE(glGetShaderiv);
        RESOLVE(glGetProgramInfoLog);
        RESOLVE(glGetShaderInfoLog);
        RESOLVE(glDeleteTextures);
        RESOLVE(glGenTextures);
        RESOLVE(glBindTexture);
        RESOLVE(glActiveTexture);
        RESOLVE(glGenerateMipmap);
        RESOLVE(glTexImage2D);
        RESOLVE(glTexParameteri);
        RESOLVE(glPixelStorei);
        RESOLVE(glEnable);
        RESOLVE(glDisable);
        RESOLVE(glGetIntegerv);
    #undef RESOLVE

        GLint stencil_bits = 0;
        GLint red_bits   = 0;
        GLint green_bits = 0;
        GLint blue_bits  = 0;
        GLint alpha_bits = 0;
        GLint depth_bits = 0;
        GLint point_size[2];
        GL_CALL(glGetIntegerv(GL_STENCIL_BITS, &stencil_bits));
        GL_CALL(glGetIntegerv(GL_RED_BITS, &red_bits));
        GL_CALL(glGetIntegerv(GL_GREEN_BITS, &green_bits));
        GL_CALL(glGetIntegerv(GL_BLUE_BITS, &blue_bits));
        GL_CALL(glGetIntegerv(GL_ALPHA_BITS, &alpha_bits));
        GL_CALL(glGetIntegerv(GL_DEPTH_BITS, &depth_bits));
        GL_CALL(glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, point_size));

        INFO("OpenGLESGraphicsDevice");
        INFO("GL %1 Vendor: %2, %3", 
            mGL.glGetString(GL_VERSION),
            mGL.glGetString(GL_VENDOR), 
            mGL.glGetString(GL_RENDERER));
        INFO("Stencil bits: %1", stencil_bits);
        INFO("Red bits: %1", red_bits);
        INFO("Blue bits: %1", blue_bits);
        INFO("Green bits: %1", green_bits);
        INFO("Alpha bits: %1", alpha_bits);
        INFO("Depth bits: %1", depth_bits);
        INFO("Point size: %1-%2", point_size[0], point_size[1]);
    }

    virtual void ClearColor(const Color4f& color) override
    {
        GL_CALL(glClearColor(color.Red(), color.Green(), color.Blue(), color.Alpha()));
        GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    }
    virtual void ClearStencil(int value) override
    {
        GL_CALL(glClearStencil(value));
        GL_CALL(glClear(GL_STENCIL_BUFFER_BIT));
    }

    virtual Shader* FindShader(const std::string& name) override
    {
        auto it = mShaders.find(name);
        if (it == std::end(mShaders))
            return nullptr;
        return it->second.get();
    }

    virtual Shader* MakeShader(const std::string& name) override
    {
        auto shader = std::make_unique<ShaderImpl>(mGL);
        auto* ret   = shader.get();
        mShaders[name] = std::move(shader);
        return ret;
    }

    virtual Program* FindProgram(const std::string& name) override
    {
        auto it = mPrograms.find(name);
        if (it == std::end(mPrograms))
            return nullptr;
        return it->second.get();
    }

    virtual Program* MakeProgram(const std::string& name) override
    {
        auto program = std::make_unique<ProgImpl>(mGL);
        auto* ret    = program.get();
        mPrograms[name] = std::move(program);
        return ret;
    }

    virtual Geometry* FindGeometry(const std::string& name) override
    {
        auto it = mGeoms.find(name);
        if (it == std::end(mGeoms))
            return nullptr;
        return it->second.get();
    }

    virtual Geometry* MakeGeometry(const std::string& name) override
    {
        auto geometry = std::make_unique<GeomImpl>(mGL);
        auto* ret = geometry.get();
        mGeoms[name] = std::move(geometry);
        return ret;
    }

    virtual Texture* FindTexture(const std::string& name) override
    {
        auto it = mTextures.find(name);
        if (it == std::end(mTextures))
            return nullptr;
        return it->second.get();
    }

    virtual Texture* MakeTexture(const std::string& name) override
    {
        auto texture = std::make_unique<TextureImpl>(mGL);
        auto* ret = texture.get();
        mTextures[name] = std::move(texture);
        return ret;
    }

    virtual void DeleteShaders()
    {
        mShaders.clear();
    }
    virtual void DeletePrograms()
    {
        mPrograms.clear();
    }

    virtual void Draw(const Program& program, const Geometry& geometry, const State& state) override
    {
        SetState(state);

        auto* myprog = (ProgImpl*)(&program);
        auto* mygeom = (GeomImpl*)(&geometry);
        myprog->SetLastUseFrameNumber(mFrameNumber);
        myprog->SetState();
        mygeom->SetLastUseFrameNumber(mFrameNumber);
        mygeom->Draw(myprog->GetName());
    }

    virtual Type GetDeviceType() const override
    { return Type::OpenGL_ES2; }
    
    virtual void CleanGarbage(size_t max_num_idle_frames) override
    {
        /* not needed atm.
        for (auto it = mPrograms.begin(); it != mPrograms.end();)
        {
            auto* impl = static_cast<ProgImpl*>(it->second.get());
            const auto last_used_frame_number = impl->GetLastUsedFrameNumber();
            if (mFrameNumber - last_used_frame_number >= max_num_idle_frames)
                it = mPrograms.erase(it);
            else ++it;
        }
        */

        for (auto it = mTextures.begin(); it != mTextures.end();)
        {
            auto* impl = static_cast<TextureImpl*>(it->second.get());
            const auto last_used_frame_number = impl->GetLastUsedFrameNumber();
            const auto is_eligible = impl->IsEligibleForGarbageCollection();
            const auto is_expired  = mFrameNumber - last_used_frame_number >= max_num_idle_frames;
            if (is_eligible && is_expired)
                it = mTextures.erase(it);
            else ++it;
        }
    }

    virtual void BeginFrame() override
    {
        for (auto& pair : mPrograms)
        {
            auto* impl = static_cast<ProgImpl*>(pair.second.get());
            impl->BeginFrame();
        }
    }
    virtual void EndFrame(bool display) override
    { 
        mFrameNumber++; 
        if (display)
            mContext->Display();
    }

private:
    bool EnableIf(GLenum flag, bool on_off)
    {
        if (on_off)
        {
            GL_CALL(glEnable(flag));
        }
        else
        {
            GL_CALL(glDisable(flag));
        }
        return on_off;
    }
    static GLenum ToGLEnum(State::StencilFunc func)
    {
        using Func = State::StencilFunc;
        switch (func)
        {
            case Func::Disabled:         return GL_NONE;
            case Func::PassAlways:       return GL_ALWAYS;
            case Func::PassNever:        return GL_NEVER;
            case Func::RefIsLess:        return GL_LESS;
            case Func::RefIsLessOrEqual: return GL_LEQUAL;
            case Func::RefIsMore:        return GL_GREATER;
            case Func::RefIsMoreOrEqual: return GL_GEQUAL;
            case Func::RefIsEqual:       return GL_EQUAL;
            case Func::RefIsNotEqual:    return GL_NOTEQUAL;
        }
        ASSERT(!"???");
        return GL_NONE;
    }
    static GLenum ToGLEnum(State::StencilOp op)
    {
        using Op = State::StencilOp;
        switch (op)
        {
            case Op::DontModify: return GL_KEEP;
            case Op::WriteZero:  return GL_ZERO;
            case Op::WriteRef:   return GL_REPLACE;
            case Op::Increment:  return GL_INCR;
            case Op::Decrement:  return GL_DECR;
        }
        ASSERT("???");
        return GL_NONE;
    }

    void SetState(const State& state)
    {
        GL_CALL(glViewport(state.viewport.GetX(), state.viewport.GetY(),
            state.viewport.GetWidth(), state.viewport.GetHeight()));

        switch (state.blending)
        {
            case State::BlendOp::None:
                {
                    GL_CALL(glDisable(GL_BLEND));
                }
                break;
            case State::BlendOp::Transparent:
                {
                    GL_CALL(glEnable(GL_BLEND));
                    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                }
                break;
            case State::BlendOp::Additive:
                {
                    GL_CALL(glEnable(GL_BLEND));
                    GL_CALL(glBlendFunc(GL_ONE, GL_ONE));
                }
                break;
        }

        if (EnableIf(GL_STENCIL_TEST, state.stencil_func != State::StencilFunc::Disabled))
        {
            const auto stencil_func  = ToGLEnum(state.stencil_func);
            const auto stencil_fail  = ToGLEnum(state.stencil_fail);
            const auto stencil_dpass = ToGLEnum(state.stencil_dpass);
            const auto stencil_dfail = ToGLEnum(state.stencil_dfail);
            GL_CALL(glStencilFunc(stencil_func, state.stencil_ref, state.stencil_mask));
            GL_CALL(glStencilOp(stencil_fail, stencil_dfail, stencil_dpass));
        }
        if (state.bWriteColor)
        {
            GL_CALL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        }
        else
        {
            GL_CALL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
        }
    }

private:
    class TextureImpl : public Texture
    {
    public:
        TextureImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {
            GL_CALL(glGenTextures(1, &mName));
            DEBUG("New texture object %1 name = %2", (void*)this, mName);

            mMinFilter = MinFilter::Mipmap;
            mMagFilter = MagFilter::Linear;
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        }
        ~TextureImpl()
        {
            GL_CALL(glDeleteTextures(1, &mName));
            DEBUG("Deleted texture %1", mName);
        }

        virtual void Upload(const void* bytes, unsigned xres, unsigned yres, Format format) override
        {
            DEBUG("Loading texture %1 %2x%3 px", mName, xres, yres);

            GLenum sizeFormat = 0;
            GLenum baseFormat = 0;
            switch (format)
            {
                case Format::RGB:
                    sizeFormat = GL_RGB;
                    baseFormat = GL_RGB;
                    break;
                case Format::RGBA:
                    sizeFormat = GL_RGBA;
                    baseFormat = GL_RGBA;
                    break;
                case Format::Grayscale:
                    // when sampling R = G = B = 0.0 and A is the alpha value from here.
                    sizeFormat = GL_ALPHA;
                    baseFormat = GL_ALPHA;
                    break;
                default: assert(!"unknown texture format."); break;
            }

            GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexImage2D(GL_TEXTURE_2D,
                0, // mip level
                sizeFormat,
                xres,
                yres,
                0, // border must be 0
                baseFormat,
                GL_UNSIGNED_BYTE,
                bytes));
            GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
            // set the default filters
            SetFilter(MinFilter::Bilinear);
            SetFilter(MagFilter::Linear);
            mWidth  = xres;
            mHeight = yres;
            mFormat = format;
        }
        virtual void SetFilter(MinFilter filter) override
        {
            mMinFilter = filter;
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getMinFilterAsGLEnum()));
        }

        virtual void SetFilter(MagFilter filter) override
        {
            mMagFilter = filter;
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getMagFilterAsGLEnum()));
        }
        virtual void SetWrapX(Wrapping w) override
        {
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 
                w == Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT));
        }
        virtual void SetWrapY(Wrapping w) override
        {
            GL_CALL(glActiveTexture(GL_TEXTURE0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, mName));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 
                w == Wrapping::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT));
        } 

        Texture::MinFilter GetMinFilter() const override
        { return mMinFilter; }

        Texture::MagFilter GetMagFilter() const override
        { return mMagFilter; }

        virtual unsigned GetWidth() const override
        { return mWidth; }

        virtual unsigned GetHeight() const override
        { return mHeight; }
        virtual Format GetFormat() const override
        { return mFormat; }

        virtual void EnableGarbageCollection(bool gc) override
        { mEnableGC = gc; }

        void bind(GLuint unit)
        {
            //GL_CALL(glBindTextureUnit(unit, m_name));
            //DEBUG("Texture %1 bound to unit %2", m_name, unit);
        }

        GLenum getMinFilterAsGLEnum() const
        {
            switch (mMinFilter)
            {
                case Texture::MinFilter::Nearest:
                    return GL_NEAREST;
                case Texture::MinFilter::Linear:
                    return GL_LINEAR;
                case Texture::MinFilter::Mipmap:
                    return GL_LINEAR_MIPMAP_LINEAR;
                case Texture::MinFilter::Bilinear:
                    return GL_LINEAR_MIPMAP_NEAREST;
                case Texture::MinFilter::Trilinear:
                    return GL_LINEAR_MIPMAP_LINEAR;
            }
            ASSERT(!"Incorrect texture minifying filter.");
            return GL_NONE;
        }

        GLenum getMagFilterAsGLEnum() const
        {
            switch (mMagFilter)
            {
                case Texture::MagFilter::Nearest:
                    return GL_NEAREST;
                case Texture::MagFilter::Linear:
                    return GL_LINEAR;
            }
            ASSERT(!"Incorrect texture magnifying filter setting.");
            return GL_NONE;
        }

        GLuint GetName() const
        { return mName; }

        void SetLastUseFrameNumber(size_t frame_number)
        { mFrameNumber = frame_number; }
        size_t GetLastUsedFrameNumber() const 
        { return mFrameNumber; }
        bool IsEligibleForGarbageCollection() const 
        { return mEnableGC; }

    private:
        const OpenGLFunctions& mGL;

        GLuint mName = 0;
    private:
        MinFilter mMinFilter = MinFilter::Nearest;
        MagFilter mMagFilter = MagFilter::Nearest;
    private:
        unsigned mWidth  = 0;
        unsigned mHeight = 0;
        Texture::Format mFormat = Texture::Format::Grayscale;
        std::size_t mFrameNumber = 0;
        bool mEnableGC = false;
    };

    class GeomImpl : public Geometry
    {
    public:
        GeomImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}

        virtual void SetDrawType(DrawType type) override
        { mDrawType = type; }

        virtual DrawType GetDrawType() const override
        { return mDrawType; }

        virtual void Update(const Vertex* verts, std::size_t count) override
        {
            mData.clear();
            mData.resize(count);
            for (size_t i=0; i<count; ++i)
            {
                mData[i] = verts[i];
            }
        }
        virtual void Update(const std::vector<Vertex>& verts) override
        {
            mData = verts;
        }
        virtual void Update(std::vector<Vertex>&& verts) override
        {
            mData = std::move(verts);
        }
        
        void Draw(GLuint program)
        {
            GLint aPosition = mGL.glGetAttribLocation(program, "aPosition");
            GLint aTexCoord = mGL.glGetAttribLocation(program, "aTexCoord");

            uint8_t* base = reinterpret_cast<uint8_t*>(&mData[0]);
            if (aPosition != -1)
            {
                GL_CALL(glVertexAttribPointer(aPosition, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aPosition))));
                GL_CALL(glEnableVertexAttribArray(aPosition));
            }
            if (aTexCoord != -1)
            {
                GL_CALL(glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE,
                    sizeof(Vertex), (void*)(base + offsetof(Vertex, aTexCoord))));
                GL_CALL(glEnableVertexAttribArray(aTexCoord));
            }
            if (mDrawType == DrawType::Triangles)
                GL_CALL(glDrawArrays(GL_TRIANGLES, 0, mData.size()));
            else if (mDrawType == DrawType::Points)
                GL_CALL(glDrawArrays(GL_POINTS, 0, mData.size()));
        }
        void SetLastUseFrameNumber(size_t frame_number)
        { mFrameNumber = frame_number; }        
    private:
        const OpenGLFunctions& mGL;
    private:
        std::size_t mFrameNumber = 0;
        std::vector<Vertex> mData;
        DrawType mDrawType = DrawType::Triangles;
    };

    class ProgImpl : public Program
    {
    public:
        ProgImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}

       ~ProgImpl()
        {
            if (mProgram)
            {
                GL_CALL(glDeleteProgram(mProgram));
            }
        }
        virtual bool Build(const std::vector<const Shader*>& shaders) override
        {
            GLuint prog = mGL.glCreateProgram();
            DEBUG("New program %1", prog);

            for (const auto* shader : shaders)
            {
                GL_CALL(glAttachShader(prog,
                    static_cast<const ShaderImpl*>(shader)->GetName()));
            }
            GL_CALL(glLinkProgram(prog));
            GL_CALL(glValidateProgram(prog));

            GLint link_status = 0;
            GLint valid_status = 0;
            GL_CALL(glGetProgramiv(prog, GL_LINK_STATUS, &link_status));
            GL_CALL(glGetProgramiv(prog, GL_VALIDATE_STATUS, &valid_status));

            GLint length = 0;
            GL_CALL(glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &length));

            std::string build_info;
            build_info.resize(length);
            GL_CALL(glGetProgramInfoLog(prog, length, nullptr, &build_info[0]));

            if ((link_status == 0) || (valid_status == 0))
            {
                ERROR("Program build error: %1", build_info);
                GL_CALL(glDeleteProgram(prog));
                return false;
            }

            DEBUG("Program was built succesfully!");
            DEBUG("Program info: %1", build_info);
            if (mProgram)
            {
                GL_CALL(glDeleteProgram(mProgram));
                GL_CALL(glUseProgram(0));
            }
            mProgram = prog;
            mVersion++;
            return true;
        }
        virtual bool IsValid() const override
        { return mProgram != 0; }

        virtual void SetUniform(const char* name, float x) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));
            GL_CALL(glUniform1f(ret, x));
        }
        virtual void SetUniform(const char* name, float x, float y) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));
            GL_CALL(glUniform2f(ret, x, y));
        }
        virtual void SetUniform(const char* name, float x, float y, float z) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));                
            GL_CALL(glUniform3f(ret, x, y, z));
        }
        virtual void SetUniform(const char* name, float x, float y, float z, float w) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));                
            GL_CALL(glUniform4f(ret, x, y, z, w));
        }
        virtual void SetUniform(const char* name, const Color4f& color) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));
            GL_CALL(glUniform4f(ret, color.Red(), color.Green(), color.Blue(), color.Alpha()));
        }
        virtual void SetUniform(const char* name, const Matrix2x2& matrix) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));                
            GL_CALL(glUniformMatrix2fv(ret, 1, GL_FALSE /* transpose */, (const float*)&matrix));
        }
        virtual void SetUniform(const char* name, const Matrix3x3& matrix) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));                
            GL_CALL(glUniformMatrix3fv(ret, 1, GL_FALSE /*transpose*/, (const float*)&matrix));
        }
        virtual void SetUniform(const char* name, const Matrix4x4& matrix) override
        {
            auto ret = mGL.glGetUniformLocation(mProgram, name);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));                
            GL_CALL(glUniformMatrix4fv(ret, 1, GL_FALSE /*transpose*/, (const float*)&matrix));
        }

        virtual void SetTexture(const char* sampler, unsigned unit, const Texture& texture) override
        {
            const auto& texture_impl = dynamic_cast<const TextureImpl&>(texture);
            const auto  texture_name = texture_impl.GetName();

            auto ret =  mGL.glGetUniformLocation(mProgram, sampler);
            if (ret == -1)
                return;
            GL_CALL(glUseProgram(mProgram));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_name));
            GL_CALL(glUniform1i(ret, unit));

            // in OpenGL the expected memory layout of texture data that
            // is given to glTexImage2D doesn't match the "typical" layout
            // that is used by many toolkits/libraries. The order of scan lines
            // is reversed and glTexImage expects the first scanline (in memory)
            // to be the bottom scanline of the image.
            // There are two ways to deal with this dilemma:
            // - flip all images on the horizontal axis before calling glTexImage2D.
            // - use a matrix to transform the texture coordinates to counter this.
            // 
            // If the program being used to render stuff is using a texture 
            // we helpfully setup here a "device texture matrix" that will be provided
            // for the program and should be used to transform the texture coordinates
            // before sampling textures.
            // This will avoid having to do any image flips which is especilly
            // handy when dealing with data that gets re-uploaded often 
            // I.e. dynamically changing/procedural texture data.
            // 
            // It should also be possible to use the device texture matrix for example
            // in cases where the device would bake some often used textures into an atlas
            // and implicitly alter the texture coordinates.
            static const float kDeviceTextureMatrix[3][3] = {
                {1.0f, 0.0f, 0.0f},
                {0.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}
            };
            SetUniform("kDeviceTextureMatrix", kDeviceTextureMatrix);

            // keep track of textures being used so that if/when this
            // program is actually used to draw stuff we can realize
            // which textures have actually been used to draw.
            mFrameTextures.push_back(const_cast<TextureImpl*>(&texture_impl));
        }

        void SetState() //const
        {
            GL_CALL(glUseProgram(mProgram));
        }
        GLuint GetName() const
        { return mProgram; }

        void SetLastUseFrameNumber(size_t frame_number)
        { 
            mFrameNumber = frame_number; 
            for (auto* texture : mFrameTextures)
            {
                texture->SetLastUseFrameNumber(frame_number);
            }
        }
        void BeginFrame()
        {
            mFrameTextures.clear();
        }
        size_t GetLastUsedFrameNumber() const 
        { return mFrameNumber; }

    private:
        const OpenGLFunctions& mGL;

    private:
        GLuint mProgram = 0;
        GLuint mVersion = 0;
        std::vector<TextureImpl*> mFrameTextures;
        std::size_t mFrameNumber = 0;
    };

    class ShaderImpl : public Shader
    {
    public:
        ShaderImpl(const OpenGLFunctions& funcs) : mGL(funcs)
        {}

       ~ShaderImpl()
        {
            if (mShader)
            {
                GL_CALL(glDeleteShader(mShader));
            }
        }
        virtual bool CompileFile(const std::string& file) override
        {
            std::ifstream stream;
            stream.open("shaders/es2/" + file);
            if (!stream.is_open())
                throw std::runtime_error("failed to open file: " + file);

            const std::string source(std::istreambuf_iterator<char>(stream), {});

            return CompileSource(source);
        }

        virtual bool CompileSource(const std::string& source) override
        {
            GLenum type = GL_NONE;
            std::stringstream ss(source);
            std::string line;
            while (std::getline(ss, line) && type == GL_NONE)
            {
                if (line.find("gl_Position") != std::string::npos)
                    type = GL_VERTEX_SHADER;
                else if (line.find("gl_FragColor") != std::string::npos)
                    type = GL_FRAGMENT_SHADER;
            }
            ASSERT(type != GL_NONE);

            GLint status = 0;
            GLint shader = mGL.glCreateShader(type);
            DEBUG("New shader %1 %2", shader, GLEnumToStr(type));

            const char* source_ptr = source.c_str();
            GL_CALL(glShaderSource(shader, 1, &source_ptr, nullptr));
            GL_CALL(glCompileShader(shader));
            GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

            GLint length = 0;
            GL_CALL(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

            std::string compile_info;
            compile_info.resize(length);
            GL_CALL(glGetShaderInfoLog(shader, length, nullptr, &compile_info[0]));

            if (status == 0)
            {
                GL_CALL(glDeleteShader(shader));
                ERROR("Shader compile error %1", compile_info);
                return false;
            }
            else
            {
                DEBUG("Shader was built succesfully!");
                DEBUG("Shader info: %1", compile_info);
            }

            if (mShader)
            {
                GL_CALL(glDeleteShader(mShader));
            }
            mShader = shader;
            mVersion++;
            return true;
        }
        virtual bool IsValid() const override
        { return mShader != 0; }

        GLuint GetName() const
        { return mShader; }

    private:
        const OpenGLFunctions& mGL;

    private:
        GLuint mShader  = 0;
        GLuint mVersion = 0;
    };
private:
    std::map<std::string, std::unique_ptr<Geometry>> mGeoms;
    std::map<std::string, std::unique_ptr<Shader>> mShaders;
    std::map<std::string, std::unique_ptr<Program>> mPrograms;
    std::map<std::string, std::unique_ptr<Texture>> mTextures;
    std::shared_ptr<Context> mContext;
    std::size_t mFrameNumber = 0;
    OpenGLFunctions mGL;
};

// static
std::shared_ptr<Device> Device::Create(Type type, 
    std::shared_ptr<Device::Context> context)
{
    return std::make_shared<OpenGLES2GraphicsDevice>(context);
}


} // namespace

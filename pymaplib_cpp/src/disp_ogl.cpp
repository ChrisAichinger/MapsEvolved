#include <memory>
#include <list>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <assert.h>

#include <Windows.h>        // Not used directly but needed for OpenGL includes
#include <GL/gl.h>          // OpenGL header file
#include <GL/glu.h>         // OpenGL utilities header file

#include "disp_ogl.h"
#include "tiles.h"
#include "winwrap.h"
#include "external/glext.h"

enum OGL_Rect { GLR_TOP = 0, GLR_LEFT, GLR_WIDTH, GLR_HEIGHT, GLR_ARRAYLEN };

static bool functions_loaded = false;
static PFNGLBLENDCOLORPROC glBlendColor = nullptr;
static PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = nullptr;
static PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = nullptr;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
static PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;

template <typename T>
void LoadOneEntryPoint(const char* name, T &target) {
    if (!target)
        target = reinterpret_cast<T>(wglGetProcAddress(name));
    if (!target)
        throw std::runtime_error(std::string("Could not access ") +
                                 std::string(name) +
                                 std::string(" to initialize OpenGL"));
}

void LoadOGLEntryPoints() {
    if (functions_loaded)
        return;

    LoadOneEntryPoint("glBlendColor", glBlendColor);
    LoadOneEntryPoint("glGenRenderbuffers", glGenRenderbuffers);
    LoadOneEntryPoint("glBindRenderbuffer", glBindRenderbuffer);
    LoadOneEntryPoint("glRenderbufferStorage", glRenderbufferStorage);
    LoadOneEntryPoint("glGenFramebuffers", glGenFramebuffers);
    LoadOneEntryPoint("glBindFramebuffer", glBindFramebuffer);
    LoadOneEntryPoint("glFramebufferRenderbuffer", glFramebufferRenderbuffer);
    LoadOneEntryPoint("glCheckFramebufferStatus", glCheckFramebufferStatus);
    LoadOneEntryPoint("glDeleteRenderbuffers", glDeleteRenderbuffers);
    LoadOneEntryPoint("glDeleteFramebuffers", glDeleteFramebuffers);
    functions_loaded = true;
}


class OGLDisplayCoord {
    public:
        OGLDisplayCoord() : x(0), y(0) {};
        OGLDisplayCoord(double x_, double y_) : x(x_), y(y_) {};
        OGLDisplayCoord(const DisplayCoordCentered &coord,
                        const class DisplayDelta &disp_size)
            // Invert y as we use top-down coords while OGL uses bottom-up
            : x(2 * coord.x / disp_size.x),
              y(-2 * coord.y / disp_size.y)
            {};

        void TexVertex2d(double tex_s, double tex_t) {
            glTexCoord2d(tex_s, tex_t);
            glVertex2d(x, y);
        };

        double x, y;
};


class Texture {
    public:
        Texture(unsigned int width, unsigned int height,
                const unsigned int *pixels, ODMPixelFormat format);
        ~Texture();

        void Activate();
        void Deactivate();

        unsigned int GetWidth() const { return m_width; };
        unsigned int GetHeight() const { return m_height; };

    private:
        DISALLOW_COPY_AND_ASSIGN(Texture);

        void MakeTexture(const unsigned int *pixels, ODMPixelFormat format);

        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_texhandle;
};


class TextureCache {
    DISALLOW_COPY_AND_ASSIGN(TextureCache);
    public:
        typedef std::shared_ptr<unsigned int> key_type;
        typedef std::shared_ptr<Texture> mapped_type;

        TextureCache() : m_cache() { };

        /** Insert a texture into the cache. */
        void Insert(const key_type &key, const mapped_type &texture) {
            auto v = cache_type::value_type(internal_key_type(key), texture);
            m_cache.insert(v);
        }

        /** Try to get a cached texture for a PixelBuf. */
        mapped_type Get(const key_type& key) {
            auto it = m_cache.find(internal_key_type(key));
            if (it == m_cache.end()) {
                return mapped_type();
            }

            // internal_key_type sorts by the address of its pointee.
            // Lock the weak_ptr and check against the requested key to
            // ensure a reused address doesn't result in bogus lookups.
            if (it->first.lock() != key) {
                m_cache.erase(it);
                return mapped_type();
            }

            return it->second;
        }

        /** Remove Textures for expired PixelBuf's objects from the cache. */
        void Clean() {
            for(auto it = m_cache.begin(); it != m_cache.end();) {
                if (it->first.expired()) {
                    m_cache.erase(it++);
                } else {
                    ++it;
                }
            }
        }

    private:
        /** A sortable wrapper around of `weak_ptr` */
        class SortableWeakPtr {
        public:
            explicit SortableWeakPtr(const key_type &p)
                : m_ptr(p), m_sortkey(p.get())
            {}
            bool operator<(const TextureCache::SortableWeakPtr& rhs) const {
                return m_sortkey < rhs.m_sortkey;
            }
            bool expired() const { return m_ptr.expired(); }
            std::shared_ptr<unsigned int> lock() const { return m_ptr.lock(); }

        private:
            std::weak_ptr<unsigned int> m_ptr;
            void *m_sortkey;
        };

        typedef SortableWeakPtr internal_key_type;
        typedef std::map<const SortableWeakPtr, mapped_type> cache_type;

        cache_type m_cache;
};


void EnsureFrameBufferStatusOK() {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status == GL_FRAMEBUFFER_COMPLETE) {
        return;
    }
    std::string errormsg = "framebuffer error: ";
    switch(status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            errormsg += "GL_FRAMEBUFFER_UNDEFINED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            errormsg += "GL_FRAMEBUFFER_UNSUPPORTED";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            errormsg += "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
            break;
        default:
            errormsg += num_to_hex(status);
    }
    throw std::runtime_error(errormsg);
}

void EnsureNoOGLError() {
    GLenum gl_error = glGetError();
    if (gl_error == GL_NO_ERROR) {
        return;
    }
    std::string errormsg = "OpenGL operation failed: ";
    switch (gl_error) {
        case GL_INVALID_ENUM: errormsg += "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: errormsg += "GL_INVALID_VALUE"; break;
        case GL_OUT_OF_MEMORY: errormsg += "GL_OUT_OF_MEMORY"; break;
        case GL_STACK_UNDERFLOW: errormsg += "GL_STACK_UNDERFLOW"; break;
        case GL_STACK_OVERFLOW: errormsg += "GL_STACK_OVERFLOW"; break;
        case GL_INVALID_OPERATION: errormsg += "GL_INVALID_OPERATION"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            errormsg += "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        default:
            errormsg += num_to_hex(gl_error);
    }
    throw std::runtime_error(errormsg);
}


class OGLFrameBufferObject {
    // OpenGL Framebuffer resource class.
    public:
        OGLFrameBufferObject() : m_handle(0)
        {
            LoadOGLEntryPoints();
            glGenFramebuffers(1, &m_handle);
            glBindFramebuffer(GL_FRAMEBUFFER, m_handle);
        }
        ~OGLFrameBufferObject() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &m_handle);
        }
        unsigned int Get() { return m_handle; };
    private:
        GLuint m_handle;
        DISALLOW_COPY_AND_ASSIGN(OGLFrameBufferObject);
};

class OGLRenderBufferObject {
    // OpenGL RenderBuffer resource class.
    public:
        OGLRenderBufferObject(GLenum format,
                              unsigned int width,
                              unsigned int height)
        : m_handle(0)
        {
            LoadOGLEntryPoints();
            glGenRenderbuffers(1, &m_handle);
            glBindRenderbuffer(GL_RENDERBUFFER, m_handle);
            glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
        }
        ~OGLRenderBufferObject() {
            glDeleteRenderbuffers(1, &m_handle);
        }
        GLuint Get() { return m_handle; };
    private:
        GLuint m_handle;
        DISALLOW_COPY_AND_ASSIGN(OGLRenderBufferObject);
};

class OGLTemporaryViewport {
    // Temporarily change the viewport to 0/0/width/height.
    // Restore the old viewport on deallocation.
    public:
        OGLTemporaryViewport(unsigned int width, unsigned int height) {
            glGetIntegerv(GL_VIEWPORT, m_orig_rect);
            glViewport(0, 0, width, height);
        }
        ~OGLTemporaryViewport() {
            glViewport(m_orig_rect[GLR_TOP], m_orig_rect[GLR_LEFT],
                       m_orig_rect[GLR_WIDTH], m_orig_rect[GLR_HEIGHT]);
        }
    private:
        int m_orig_rect[GLR_ARRAYLEN];
        DISALLOW_COPY_AND_ASSIGN(OGLTemporaryViewport);
};


DispOpenGL::DispOpenGL(const std::shared_ptr<OGLContext> &ogl_context)
    : m_opengl(ogl_context),
      m_texcache(new TextureCache()),
      m_orders()
{
    LoadOGLEntryPoints();
}

void DispOpenGL::SetDisplaySize(const DisplayDeltaInt &new_size) {
    glViewport(0, 0, new_size.x, new_size.y);
}

void DispOpenGL::ForceRepaint() {
    m_opengl->GetDevContext()->ForceRepaint();
}

void
DispOpenGL::Render(const std::list<std::shared_ptr<DisplayOrder>> &orders) {
    m_orders = orders;
    Redraw();
}

void DispOpenGL::Redraw() {
    m_texcache->Clean();
    DisplayDelta target_size(GetDisplaySize());

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);

    for (auto it=m_orders.cbegin(); it != m_orders.cend(); ++it) {
        auto dorder = *it;
        const PixelPromise &promise = dorder->GetPixelBufPromise();
        auto pixels = promise.GetPixels();
        auto pixeldata = pixels.GetData();

        std::shared_ptr<Texture> tex;
        if (pixeldata) {
            tex = m_texcache->Get(pixels.GetData());
        }

        if (!tex) {
            tex.reset(new Texture(pixels.GetWidth(),
                                  pixels.GetHeight(),
                                  pixeldata.get(),
                                  promise.GetPixelFormat()));
            if (pixeldata) {
                m_texcache->Insert(pixels.GetData(), tex);
            }
        }

        switch (promise.GetPixelFormat()) {
            case ODM_PIX_RGBA4:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case ODM_PIX_RGBX4:
                glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
                glBlendColor(
                        0.0f, 0.0f, 0.0f,
                        static_cast<GLfloat>(1.0 - dorder->GetTransparency()));
                break;
            default:
                assert(false);
        }

        tex->Activate();
        glBegin(GL_QUADS);
        const DisplayRectCentered& drect = dorder->GetDisplayRect();
        OGLDisplayCoord(drect.br, target_size).TexVertex2d(1, 0);
        OGLDisplayCoord(drect.bl, target_size).TexVertex2d(0, 0);
        OGLDisplayCoord(drect.tl, target_size).TexVertex2d(0, 1);
        OGLDisplayCoord(drect.tr, target_size).TexVertex2d(1, 1);
        glEnd();
        tex->Deactivate();
    }
    glFlush();
    glDisable(GL_BLEND);

    EnsureNoOGLError();
}

unsigned int DispOpenGL::GetDisplayWidth() const {
    int rect[GLR_ARRAYLEN];
    glGetIntegerv(GL_VIEWPORT, rect);
    return rect[GLR_WIDTH];
}

unsigned int DispOpenGL::GetDisplayHeight() const {
    int rect[GLR_ARRAYLEN];
    glGetIntegerv(GL_VIEWPORT, rect);
    return rect[GLR_HEIGHT];
}

DisplayDeltaInt DispOpenGL::GetDisplaySize() const {
    int rect[GLR_ARRAYLEN];
    glGetIntegerv(GL_VIEWPORT, rect);
    return DisplayDeltaInt(rect[GLR_WIDTH], rect[GLR_HEIGHT]);
}

PixelBuf DispOpenGL::RenderToBuffer(
        ODMPixelFormat format,
        unsigned int width, unsigned int height,
        std::list<std::shared_ptr<DisplayOrder>> &orders)
{
    // A good overview of Framebuffer objects can be found at:
    // http://www.songho.ca/opengl/gl_fbo.html
    OGLFrameBufferObject m_framebuf;
    OGLRenderBufferObject m_renderbuf_color(GL_RGBA8, width, height);

    // Add a color renderbuffer to currently bound framebuffer.
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, m_renderbuf_color.Get());

    EnsureFrameBufferStatusOK();

    {
        OGLTemporaryViewport temp_viewport(width, height);
        Render(orders);
    }

    PixelBuf result(width, height);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 result.GetRawData());
    EnsureNoOGLError();
    return result;
}


Texture::Texture(unsigned int width, unsigned int height,
                 const unsigned int *pixels, ODMPixelFormat format)
    : m_width(width), m_height(height)
{
    MakeTexture(pixels, format);
}

void Texture::MakeTexture(const unsigned int *pixels, ODMPixelFormat format) {
    static_assert(std::is_same<GLuint, decltype(m_texhandle)>::value,
                  "m_texhandle must be compatible to GLuint");

    glGenTextures(1, &m_texhandle);
    glBindTexture(GL_TEXTURE_2D, m_texhandle);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    switch (format) {
        case ODM_PIX_RGBA4:
        case ODM_PIX_RGBX4:
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // No padding in pixels
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            break;
        default:
            assert(false); // Not implemented
    }
    EnsureNoOGLError();
}

Texture::~Texture() {
    glDeleteTextures(1, &m_texhandle);
}

void Texture::Activate() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texhandle);
}

void Texture::Deactivate() {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

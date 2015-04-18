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


DisplayCoordCentered CenteredCoordFromDisplay(const DisplayCoord& dc,
                                              const Display& disp)
{
    return DisplayCoordCentered(dc.x - disp.GetDisplayWidth() / 2.0,
                                dc.y - disp.GetDisplayHeight() / 2.0);
}
DisplayCoord DisplayCoordFromCentered(const DisplayCoordCentered& dc,
                                      const Display& disp)
{
    return DisplayCoord(dc.x + disp.GetDisplayWidth() / 2.0,
                        dc.y + disp.GetDisplayHeight() / 2.0);
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


class TextureCache {
    public:
        TextureCache() : m_cache(), m_call_count() { };
        void Insert(const class TileCode& tc,
                    const std::shared_ptr<class Texture> &texture) {
            ++m_call_count;
            struct LruTexture val = { m_call_count, texture };
            m_cache[tc] = val;

            if (m_cache.size() > TEXCACHE_SIZE) {
                // Too many elements, drop one
                auto oldest = *std::max_element(
                          m_cache.begin(), m_cache.end(),
                          [=](const CachePair& lhs, const CachePair& rhs) {
                                 return (m_call_count - lhs.second.last_used) <
                                        (m_call_count - rhs.second.last_used);
                          });
                m_cache.erase(oldest.first);
            }
        }

        // The internal texture may be dropped at any time, so return by value
        std::shared_ptr<class Texture> Get(const class TileCode& tc) {
            ++m_call_count;
            if (m_cache.count(tc) == 0)
                return std::shared_ptr<class Texture>();
            m_cache[tc].last_used = m_call_count;
            return m_cache[tc].texture;
        }

    private:
        DISALLOW_COPY_AND_ASSIGN(TextureCache);

        static const int TEXCACHE_SIZE = 200;
        struct LruTexture {
            unsigned int last_used;
            std::shared_ptr<class Texture> texture;
        };
        typedef std::pair<const class TileCode, struct LruTexture> CachePair;

        std::map<const class TileCode, struct LruTexture> m_cache;
        unsigned int m_call_count;
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
      m_texcache(new TextureCache())
{
    LoadOGLEntryPoints();
}

void DispOpenGL::Resize(unsigned int width, unsigned int height) {
    glViewport(0, 0, width, height);
}

void DispOpenGL::ForceRepaint() {
    m_opengl->GetDevContext()->ForceRepaint();
}

void DispOpenGL::Render(std::list<std::shared_ptr<DisplayOrder>> &orders) {
    DisplayDelta target_size(GetDisplaySize());

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);

    for (auto it=orders.cbegin(); it != orders.cend(); ++it) {
        auto dorder = *it;
        const TileCode *tilecode = dorder->GetTileCode();
        std::shared_ptr<Texture> tex;
        if (dorder->IsCachable()) {
            tex = m_texcache->Get(*tilecode);
        }

        if (!tex) {
            PixelBuf pixbuf = dorder->GetPixels();
            tex.reset(new Texture(pixbuf.GetWidth(),
                                  pixbuf.GetHeight(),
                                  pixbuf.GetRawData(),
                                  dorder->GetPixelFormat()));
            if (dorder->IsCachable()) {
                m_texcache->Insert(*tilecode, tex);
            }
        }

        switch (dorder->GetPixelFormat()) {
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
        OGLDisplayCoord(dorder->GetBotRight(), target_size).TexVertex2d(1, 0);
        OGLDisplayCoord(dorder->GetBotLeft(), target_size).TexVertex2d(0, 0);
        OGLDisplayCoord(dorder->GetTopLeft(), target_size).TexVertex2d(0, 1);
        OGLDisplayCoord(dorder->GetTopRight(), target_size).TexVertex2d(1, 1);
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

DisplayDelta DispOpenGL::GetDisplaySize() const {
    int rect[GLR_ARRAYLEN];
    glGetIntegerv(GL_VIEWPORT, rect);
    return DisplayDelta(rect[GLR_WIDTH], rect[GLR_HEIGHT]);
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

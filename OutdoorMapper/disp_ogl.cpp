
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

static PFNGLBLENDCOLORPROC glBlendColor = 0;

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

DispOpenGL::DispOpenGL(const std::shared_ptr<OGLContext> &ogl_context)
    : m_opengl(ogl_context),
      m_texcache(new TextureCache())
{
    if (!glBlendColor)
        glBlendColor = (PFNGLBLENDCOLORPROC)wglGetProcAddress("glBlendColor");
    if (!glBlendColor)
        throw std::runtime_error(
                "Could not access glBlendColor to initialize OpenGL");
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
    glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);

    for (auto it=orders.cbegin(); it != orders.cend(); ++it) {
        auto dorder = *it;
        const TileCode *tilecode = dorder->GetTileCode();
        std::shared_ptr<Texture> tex;
        if (dorder->IsCachable()) {
            tex = m_texcache->Get(*tilecode);
            if (!tex) {
                std::shared_ptr<unsigned int> pixels(dorder->GetPixels());
                tex.reset(new Texture(dorder->GetPixelSize().x,
                                      dorder->GetPixelSize().y, pixels.get()));
                m_texcache->Insert(*tilecode, tex);
            }
        } else {
            std::shared_ptr<unsigned int> pixels(dorder->GetPixels());
            tex.reset(new Texture(dorder->GetPixelSize().x,
                                  dorder->GetPixelSize().y, pixels.get()));
        }

        glBlendColor(0.0f, 0.0f, 0.0f,
                     static_cast<GLfloat>(1.0 - dorder->GetTransparency()));

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

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL operation failed.");
    }
}

enum OGL_Rect { GLR_TOP = 0, GLR_LEFT, GLR_WIDTH, GLR_HEIGHT, GLR_ARRAYLEN };

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

Texture::Texture(unsigned int width, unsigned int height,
                 const unsigned int *pixels)
    : m_width(width), m_height(height)
{
    MakeTexture(pixels);
}

void Texture::MakeTexture(const unsigned int *pixels) {
    glGenTextures(1, &m_texhandle);
    glBindTexture(GL_TEXTURE_2D, m_texhandle);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // No padding in pixels
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    assert(glGetError() == 0);
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

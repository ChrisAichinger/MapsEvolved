
#include <memory>
#include <vector>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <assert.h>

#include <Windows.h>
#include <GL/gl.h>                      /* OpenGL header file */
#include <GL/glu.h>                     /* OpenGL utilities header file */

#include "disp_ogl.h"
#include "tiles.h"
#include "winwrap.h"
#include "external/glext.h"


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
{ }

void DispOpenGL::Resize(unsigned int width, unsigned int height) {
    glViewport(0, 0, width, height);
}

void DispOpenGL::ForceRepaint() {
    m_opengl->GetDevContext()->ForceRepaint();
}

void DispOpenGL::Render(std::vector<class DisplayOrder> &orders) {
    unsigned int target_width = GetDisplayWidth();
    unsigned int target_height = GetDisplayHeight();

    glClear(GL_COLOR_BUFFER_BIT);

    for (auto it=orders.begin(); it < orders.end(); ++it) {
        std::shared_ptr<Texture> tex = m_texcache->Get(it->GetTileCode());
        if (!tex) {
            tex.reset(new Texture(it->GetTileCode()));
            m_texcache->Insert(it->GetTileCode(), tex);
        }

        double x = 2 * it->GetX() / target_width;
        double width = 2 * it->GetWidth() / target_width;
        double y = -2 * it->GetY() / target_height;
        double height = -2 * it->GetHeight() / target_height;

        tex->Activate();
        glBegin(GL_QUADS);
        glTexCoord2d(1, 0); glVertex2d(x + width, y + height);
        glTexCoord2d(0, 0); glVertex2d(x, y + height);
        glTexCoord2d(0, 1); glVertex2d(x, y);
        glTexCoord2d(1, 1); glVertex2d(x + width, y);
        glEnd();
        tex->Deactivate();
    }
    glFlush();

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

Texture::Texture(unsigned int width, unsigned int height,
                 const unsigned int *pixels)
    : m_width(width), m_height(height)
{
    MakeTexture(pixels);
}

Texture::Texture(const class TileCode& tilecode)
    : m_width(tilecode.GetTileSize()), m_height(tilecode.GetTileSize()) {

    std::shared_ptr<unsigned int> pixels(tilecode.GetTile());
    MakeTexture(pixels.get());
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

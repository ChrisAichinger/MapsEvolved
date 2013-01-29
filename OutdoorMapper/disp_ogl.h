#ifndef ODM__DISP_OGL_H
#define ODM__DISP_OGL_H

#include <vector>
#include <memory>

#include "util.h"

class DevContext;
class OGLContext;
class TextureCache;

class DispOpenGL {
    public:
        explicit DispOpenGL(const std::shared_ptr<OGLContext> &ogl_context);

        unsigned int GetDisplayWidth() const;
        unsigned int GetDisplayHeight() const;

        void Render(class std::vector<class DisplayOrder> &orders);
        void Resize(unsigned int width, unsigned int height);
        void ForceRepaint();

    private:
        DISALLOW_COPY_AND_ASSIGN(DispOpenGL);

        std::shared_ptr<OGLContext> m_opengl;
        std::shared_ptr<class TextureCache> m_texcache;
};

class Texture {
    public:
        Texture(unsigned int width, unsigned int height,
                const unsigned int *pixels);
        explicit Texture(const class TileCode& tilecode);
        ~Texture();

        void Activate();
        void Deactivate();

        unsigned int GetWidth() const { return m_width; };
        unsigned int GetHeight() const { return m_height; };

    private:
        DISALLOW_COPY_AND_ASSIGN(Texture);

        void MakeTexture(const unsigned int *pixels);

        unsigned int m_width;
        unsigned int m_height;
        unsigned int m_texhandle;
};

#endif

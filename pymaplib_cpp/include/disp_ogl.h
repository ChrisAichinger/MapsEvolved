#ifndef ODM__DISP_OGL_H
#define ODM__DISP_OGL_H

#include <list>
#include <memory>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "pixelbuf.h"
#include "display.h"
#include "tiles.h"


class DevContext;
class OGLContext;
class TextureCache;

class EXPORT DispOpenGL : public Display {
    public:
        explicit DispOpenGL(const std::shared_ptr<OGLContext> &ogl_context);

        virtual unsigned int GetDisplayWidth() const;
        virtual unsigned int GetDisplayHeight() const;
        virtual DisplayDelta GetDisplaySize() const;

        virtual void Render(
                const class std::list<std::shared_ptr<DisplayOrder>> &orders);
        virtual void Resize(unsigned int width, unsigned int height);
        virtual void ForceRepaint();

        virtual PixelBuf
        RenderToBuffer(ODMPixelFormat format,
                       unsigned int width, unsigned int height,
                       std::list<std::shared_ptr<DisplayOrder>> &orders);

    private:
        DISALLOW_COPY_AND_ASSIGN(DispOpenGL);

        const std::shared_ptr<OGLContext> m_opengl;
        const std::shared_ptr<class TextureCache> m_texcache;
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

#endif

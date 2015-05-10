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
        virtual DisplayDeltaInt GetDisplaySize() const;
        virtual void SetDisplaySize(const DisplayDeltaInt &new_size);

        virtual void Render(
                const class std::list<std::shared_ptr<DisplayOrder>> &orders);
        virtual void Redraw();
        virtual void ForceRepaint();

        virtual PixelBuf
        RenderToBuffer(ODMPixelFormat format,
                       unsigned int width, unsigned int height,
                       std::list<std::shared_ptr<DisplayOrder>> &orders);

    private:
        DISALLOW_COPY_AND_ASSIGN(DispOpenGL);

        const std::shared_ptr<OGLContext> m_opengl;
        const std::shared_ptr<class TextureCache> m_texcache;
        std::list<std::shared_ptr<DisplayOrder>> m_orders;
};

#endif

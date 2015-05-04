#ifndef ODM__DISPLAY_H
#define ODM__DISPLAY_H

#include <list>
#include <memory>

#include "odm_config.h"
#include "coordinates.h"
#include "pixelbuf.h"


class EXPORT Display {
    public:
        virtual unsigned int GetDisplayWidth() const = 0;
        virtual unsigned int GetDisplayHeight() const = 0;
        virtual DisplayDelta GetDisplaySize() const = 0;

        // Render a new set of display orders.
        virtual void Render(
            const class std::list<std::shared_ptr<class DisplayOrder>> &orders) = 0;

        // Redraw the current display orders.
        virtual void Redraw() = 0;
        virtual void Resize(unsigned int width, unsigned int height) = 0;
        virtual void ForceRepaint() = 0;

        virtual PixelBuf
        RenderToBuffer(ODMPixelFormat format,
                       unsigned int width, unsigned int height,
                       std::list<std::shared_ptr<DisplayOrder>> &orders) = 0;
};

EXPORT DisplayCoordCentered CenteredCoordFromDisplay(
    const DisplayCoord& dc, const Display& disp);

EXPORT DisplayCoord DisplayCoordFromCentered(
    const DisplayCoordCentered& dc, const Display& disp);

#endif

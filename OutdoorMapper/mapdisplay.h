#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include "util.h"
#include "coordinates.h"

class MapDisplayManager {
    public:
        MapDisplayManager(std::shared_ptr<class DispOpenGL> &display,
                          const class RasterMapCollection &maps);

        const class RasterMap &GetBaseMap() const;
        double GetZoom() const;
        double GetCenterX() const;
        double GetCenterY() const;
        const MapPixelCoord &GetCenter() const;

        void ChangeMap(const RasterMap *new_map, bool try_preserve_pos=true);
        void Resize(unsigned int width, unsigned int height);
        void StepZoom(double steps);
        // mouse_x, mouse_y are mouse coordinates relative to the map panel
        // The map location under the mouse is held constant
        void StepZoom(double steps, const DisplayCoord &mouse_pos);
        void DragMap(const DisplayDelta &delta);
        void CenterToDisplayCoord(const DisplayCoord &center);
        void Paint();

        MapPixelCoord MapPixelCoordFromDisplay(const DisplayCoord &disp) const;

    private:
        DISALLOW_COPY_AND_ASSIGN(MapDisplayManager);

        static const int TILE_SIZE = 512;
        static const double ZOOM_STEP;

        std::shared_ptr<class DispOpenGL> m_display;
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_base_map;

        MapPixelCoord m_center;
        double m_zoom;
};

#endif

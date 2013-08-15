#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include <list>

#include "util.h"
#include "coordinates.h"

class MapDisplayManager {
    public:
        MapDisplayManager(std::shared_ptr<class DispOpenGL> &display,
                          const class RasterMap &initial_map);

        const class RasterMap &GetBaseMap() const;
        double GetZoom() const;
        double GetCenterX() const;
        double GetCenterY() const;
        const BaseMapCoord &GetCenter() const;

        void ChangeMap(const RasterMap *new_map, bool try_preserve_pos=true);
        void AddOverlayMap(const RasterMap *new_map);

        void Resize(unsigned int width, unsigned int height);
        void StepZoom(double steps);
        // The map location under the mouse is held constant
        void StepZoom(double steps, const DisplayCoord &mouse_pos);
        void SetZoomOneToOne();
        void DragMap(const DisplayDelta &delta);
        void CenterToDisplayCoord(const DisplayCoord &center);
        void Paint();

        BaseMapCoord BaseCoordFromDisplay(const DisplayCoord &disp) const;
        BaseMapCoord
            BaseCoordFromDisplay(const DisplayCoordCentered &disp) const;

        BaseMapDelta BaseDeltaFromDisplay(const DisplayDelta &disp) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromBase(const BaseMapCoord &mpc) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoord &mpc,
                                         const RasterMap &map) const;
        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoordInt &mpc,
                                         const RasterMap &map) const;

    private:
        void PaintOneLayer(std::list<class DisplayOrder> *orders,
                           const class RasterMap &map,
                           const MapPixelCoordInt &tile_topleft,
                           const MapPixelCoordInt &tile_botright,
                           const MapPixelDeltaInt &tile_size,
                           double transparency = 0.0);

        bool AdvanceAlongBorder(MapPixelCoordInt *base_point,
                                const MapPixelCoordInt &base_tl,
                                const MapPixelCoordInt &base_br);
        bool CalcOverlayTiles(const class RasterMap *overlay_map,
                              const MapPixelCoordInt &base_tl,
                              const MapPixelCoordInt &base_br,
                              MapPixelCoordInt *overlay_tl,
                              MapPixelCoordInt *overlay_br);

        bool TryChangeMapPreservePos(const RasterMap *new_map);

        static const int TILE_SIZE = 512;
        static const double ZOOM_STEP;

        std::shared_ptr<class DispOpenGL> m_display;
        const class RasterMap *m_base_map;
        std::list<const class RasterMap *> m_overlays;

        BaseMapCoord m_center;
        double m_zoom;

        DISALLOW_COPY_AND_ASSIGN(MapDisplayManager);
};

#endif

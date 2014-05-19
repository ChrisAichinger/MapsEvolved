#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include <list>

#include "util.h"
#include "coordinates.h"
#include "pixelbuf.h"

class EXPORT OverlaySpec {
    public:
        OverlaySpec() : m_map(nullptr), m_transparency(0) {};
        OverlaySpec(const std::shared_ptr<class GeoDrawable> &map,
                    bool enabled = true,
                    float transparency = 0.5f)
            : m_map(map), m_enabled(enabled), m_transparency(transparency)
        {};
        std::shared_ptr<class GeoDrawable> GetMap() const { return m_map; }
        bool GetEnabled() const { return m_enabled; }
        float GetTransparency() const { return m_transparency; }
        void SetMap(const std::shared_ptr<class GeoDrawable> &map) {
            m_map = map;
        }
        void SetEnabled(bool enabled) { m_enabled = enabled; }
        void SetTransparency(float transparency) {
            m_transparency = transparency;
        }
    private:
        std::shared_ptr<class GeoDrawable> m_map;
        bool m_enabled;
        float m_transparency;
};

typedef std::vector<OverlaySpec> OverlayList;

class EXPORT MapDisplayManager {
    public:
        MapDisplayManager(
                const std::shared_ptr<class DispOpenGL> &display,
                const std::shared_ptr<class GeoDrawable> &initial_map);

        std::shared_ptr<class GeoDrawable> GetBaseMap() const;
        double GetZoom() const;
        double GetCenterX() const;
        double GetCenterY() const;
        void SetCenter(const BaseMapCoord &center);
        void SetCenter(const LatLon &center);
        const BaseMapCoord &GetCenter() const;

        void ChangeMap(const std::shared_ptr<class GeoDrawable> &new_map,
                       bool try_preserve_pos=true);

        void AddOverlayMap(const std::shared_ptr<class GeoDrawable> &new_map);
        const OverlayList &GetOverlayList() const { return m_overlays; };
        void SetOverlayList(OverlayList overlaylist);
        void Resize(unsigned int width, unsigned int height);
        void StepZoom(double steps);
        // The map location under the mouse is held constant
        void StepZoom(double steps, const DisplayCoord &mouse_pos);
        void SetZoomOneToOne();
        void DragMap(const DisplayDelta &delta);
        void CenterToDisplayCoord(const DisplayCoord &center);
        void Paint();
        PixelBuf PaintToBuffer(ODMPixelFormat format,
                                unsigned int width, unsigned int height);

        BaseMapCoord BaseCoordFromDisplay(const DisplayCoord &disp) const;
        BaseMapCoord
            BaseCoordFromDisplay(const DisplayCoordCentered &disp) const;

        BaseMapDelta BaseDeltaFromDisplay(const DisplayDelta &disp) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromBase(const BaseMapCoord &mpc) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoord &mpc,
                         const std::shared_ptr<class GeoDrawable> &map) const;
        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoordInt &mpc,
                         const std::shared_ptr<class GeoDrawable> &map) const;

    private:
        // Generate a list of DisplayOrders for the current position and zoom
        // level. This encompasses the basemap as well as all overlays.
        std::list<std::shared_ptr<class DisplayOrder>>
        GenerateDisplayOrders(const DisplayDelta &disp_size_d);

        // Calculate the map tiles required for filligng the display region,
        // and add display orders to show those tiles.
        // This is the default for most maps.
        void PaintLayerTiled(
                std::list<std::shared_ptr<class DisplayOrder>> *orders,
                const std::shared_ptr<class GeoDrawable> &map,
                const MapPixelCoordInt &base_pixel_topleft,
                const MapPixelCoordInt &base_pixel_botright,
                const MapPixelDeltaInt &tile_size,
                double transparency);

        // Add an order effecting GetRegionDirect(), which takes information
        // about the current projection, and returns a PixelBuf with the size
        // of the current display. That region is then shown directly on the
        // display without the need for rotation, stretching, ...
        // This is impractical for typical maps, as it requires re-reading the
        // image each time, it is however useful for displaying GPS Tracks and
        // other overlays. Moving overlays through the tile mechanism would
        // cause stretching, compromising display quality.
        void PaintLayerDirect(
                std::list<std::shared_ptr<DisplayOrder>> *orders,
                const std::shared_ptr<class GeoDrawable> &map,
                const MapPixelDelta &half_disp_size,
                double transparency);

        bool CalcOverlayTiles(
                const std::shared_ptr<class GeoDrawable> &overlay_map,
                const MapPixelDeltaInt &tile_size,
                const MapPixelCoordInt &base_tl,
                const MapPixelCoordInt &base_br,
                MapPixelCoordInt *overlay_tl,
                MapPixelCoordInt *overlay_br);

        bool TryChangeMapPreservePos(
                const std::shared_ptr<class GeoDrawable> &new_map);

        static const int TILE_SIZE = 512;
        static const double ZOOM_STEP;

        const std::shared_ptr<class DispOpenGL> m_display;
        std::shared_ptr<class GeoDrawable> m_base_map;
        OverlayList m_overlays;

        BaseMapCoord m_center;
        double m_zoom;

        DISALLOW_COPY_AND_ASSIGN(MapDisplayManager);
};

#endif

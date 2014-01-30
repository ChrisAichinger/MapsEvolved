#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include <list>

#include "util.h"
#include "coordinates.h"

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
        void PaintOneLayer(std::list<class DisplayOrder> *orders,
                           const std::shared_ptr<class GeoDrawable> &map,
                           const MapPixelCoordInt &tile_topleft,
                           const MapPixelCoordInt &tile_botright,
                           const MapPixelDeltaInt &tile_size,
                           double transparency = 0.0);

        bool AdvanceAlongBorder(MapPixelCoordInt *base_point,
                                const MapPixelCoordInt &base_tl,
                                const MapPixelCoordInt &base_br);
        bool CalcOverlayTiles(
                const std::shared_ptr<class GeoDrawable> &overlay_map,
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

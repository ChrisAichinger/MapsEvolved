#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include <list>
#include <map>

#include "util.h"
#include "coordinates.h"
#include "pixelbuf.h"
#include "tiles.h"

class GeoDrawable;

class EXPORT OverlaySpec {
    public:
        OverlaySpec() : m_map(nullptr), m_transparency(0) {};
        OverlaySpec(const std::shared_ptr<GeoDrawable> &map,
                    bool enabled = true,
                    float transparency = 0.5f)
            : m_map(map), m_enabled(enabled), m_transparency(transparency)
        {};
        std::shared_ptr<GeoDrawable> GetMap() const { return m_map; }
        bool GetEnabled() const { return m_enabled; }
        float GetTransparency() const { return m_transparency; }
        void SetMap(const std::shared_ptr<GeoDrawable> &map) {
            m_map = map;
        }
        void SetEnabled(bool enabled) { m_enabled = enabled; }
        void SetTransparency(float transparency) {
            m_transparency = transparency;
        }
    private:
        std::shared_ptr<GeoDrawable> m_map;
        bool m_enabled;
        float m_transparency;
};

typedef std::vector<OverlaySpec> OverlayList;

/** Store the all information necessary to display the map panel.
 *
 * Encapsulte what should be displayed without knowing how it is brought to the
 * screen.
 */
class EXPORT MapDisplayManager {
    public:
        MapDisplayManager(const std::shared_ptr<GeoDrawable> &initial_map,
                          const DisplayDeltaInt &display_size);

        /** Get the current zoom level.
         *
         * Higher zoom levels mean higher magnification.
         */
        double GetZoom() const;

        /** Zoom into or out of the map center.
         *
         * Positive steps zoom in, negative steps zoom out.
         */
        void StepZoom(double steps);

        /** Zoom into or out of the `mouse_pos` position.
        *
        * Change the zoom so that the location at `mouse_pos` doesn't change.
        * Positive steps zoom in, negative steps zoom out.
        */
        void StepZoom(double steps, const DisplayCoord &mouse_pos);

        /** Make one map pixel show up as one pixel on the screen. */
        void SetZoomOneToOne();

        /** Return the location currently centered on the display
         *
         *  In `BaseMapCoord`'s, that is in pixel coordinates on the base map.
         */
        const BaseMapCoord &GetCenter() const;

        /** Set the center location on display
         *
         *  In `BaseMapCoord`'s, that is in pixel coordinates on the base map.
         */
        void SetCenter(const BaseMapCoord &center);

        /** Set the center location on display
         *
         *  In `LatLon`'s, that is in geographical coordinates on the world.
         */
        void SetCenter(const LatLon &center);

        // FIXME: Should move to the controller, instead have a MoveCenter(BaseMapCoord)
        void DragMap(const DisplayDelta &delta);
        // FIXME: Should become SetCenter(DisplayCoord)
        void CenterToDisplayCoord(const DisplayCoord &center);


        std::shared_ptr<GeoDrawable> GetBaseMap() const;
        void ChangeMap(const std::shared_ptr<GeoDrawable> &new_map,
                       bool try_preserve_pos=true);

        const DisplayDeltaInt &GetDisplaySize() const { return m_display_size; }
        void SetDisplaySize(const DisplayDeltaInt &new_size);

        const OverlayList &GetOverlayList() const { return m_overlays; }
        void SetOverlayList(const OverlayList &overlay_list);

        // Utility functions
        BaseMapCoord BaseCoordFromDisplay(const DisplayCoord &disp) const;
        BaseMapCoord
        BaseCoordFromDisplay(const DisplayCoordCentered &disp) const;

        BaseMapDelta BaseDeltaFromDisplay(const DisplayDelta &disp) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromBase(const BaseMapCoord &mpc) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoord &mpc,
                         const std::shared_ptr<GeoDrawable> &map) const;
        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoordInt &mpc,
                         const std::shared_ptr<GeoDrawable> &map) const;

        unsigned int GetChangeCtr() const { return m_change_ctr; }

    private:
        bool TryChangeMapPreservePos(
                const std::shared_ptr<GeoDrawable> &new_map);

        static const double ZOOM_STEP;

        std::shared_ptr<GeoDrawable> m_base_map;
        OverlayList m_overlays;

        // The base map pixel currently shown at the center of the display.
        BaseMapCoord m_center;

        // Zoom of the base map. Larger values indicate higher "zoom".
        // Basemap pixels take up m_zoom display pixels on screen.
        double m_zoom;

        // Size of the map display on screen.
        DisplayDeltaInt m_display_size;

        // Change counter that is increased on every modification.
        unsigned int m_change_ctr;
};


class EXPORT MapView {
DISALLOW_COPY_AND_ASSIGN(MapView);
public:
    MapView(const std::shared_ptr<class Display> &display);

    void Paint(const MapDisplayManager &mdm);
    PixelBuf PaintToBuffer(ODMPixelFormat format,
                           const MapDisplayManager &mdm);

    void ForceFullRepaint();
private:
    static const int TILE_SIZE = 512;

    const std::shared_ptr<class Display> m_display;

    bool m_need_full_repaint;

    std::map<const TileCode, std::shared_ptr<class PixelPromise>
            > m_old_promise_cache, m_new_promise_cache;

    // Generate a list of DisplayOrders for the current position and zoom
    // level. This encompasses the basemap as well as all overlays.
    std::list<std::shared_ptr<class DisplayOrder>>
    GenerateDisplayOrders(const MapDisplayManager &mdm);

    // Calculate the map tiles required for filligng the display region,
    // and add display orders to show those tiles.
    // This is the default for most maps.
    void PaintLayerTiled(
        const MapDisplayManager &mdm,
        std::list<std::shared_ptr<class DisplayOrder>> *orders,
        const std::shared_ptr<GeoDrawable> &map,
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
        const MapDisplayManager &mdm,
        std::list<std::shared_ptr<DisplayOrder>> *orders,
        const std::shared_ptr<GeoDrawable> &map,
        const DisplayDelta &disp_size_d,
        const MapPixelDelta &half_disp_size,
        double transparency);

    bool CalcOverlayTiles(
        const std::shared_ptr<GeoDrawable> &base_map,
        const std::shared_ptr<GeoDrawable> &overlay_map,
        const MapPixelDeltaInt &tile_size,
        const MapPixelCoordInt &base_tl,
        const MapPixelCoordInt &base_br,
        MapPixelCoordInt *overlay_tl,
        MapPixelCoordInt *overlay_br);
};


#endif

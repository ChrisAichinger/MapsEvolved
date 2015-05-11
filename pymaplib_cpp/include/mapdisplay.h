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
class EXPORT MapViewModel {
    public:
        MapViewModel(const std::shared_ptr<GeoDrawable> &initial_map,
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

        /** Return the location currently centered on the display.
         *
         *  In `BaseMapCoord`'s, that is in pixel coordinates on the basemap.
         */
        const BaseMapCoord &GetCenter() const;

        /** Set the center location on display.
         *
         *  In `BaseMapCoord`'s, that is in pixel coordinates on the basemap.
         */
        void SetCenter(const BaseMapCoord &center);

        /** Set the center location on display.
         *
         *  In `LatLon`'s, that is in geographical coordinates on the world.
         */
        void SetCenter(const LatLon &center);

        /** Set the center location on display.
         *
         *  In `DisplayCoord`'s, i.e. move the map so that the pixel location
         *  on the screen `center` becomes the new map center.
         */
        void SetCenter(const DisplayCoord &center);

        /** Set the map center relative to the old center.
         *
         * The offset is given in `DisplayDelta` units, i.e. as number of
         * pixels on the user's screen.
         */
        void MoveCenter(const DisplayDelta &delta);

        /** Get the current basemap.
         *
         * The basemap is special because it defines the projection used for
         * all other maps. It is drawn so that one map pixel is exactly
         * on one display pixel (disregarding zoom for the moment) and "up" on
         * the map is the same as "up" on the display.
         *
         * Other maps (drawn as overlays) may have different projections. To
         * make the maps show the same real-world coordinate at the same
         * display location, the overlaid maps are reprojected to fit the
         * projection established by the basemap. This generally results in
         * lower-quality display (due to scaling/stretching/rotating).
         *
         * The basemap is also the lowest map in the z-order on display, all
         * overlays draw over it.
         */
        std::shared_ptr<GeoDrawable> GetBaseMap() const;

        /** Set a new basemap.
         *
         * If `try_preserve_pos` is `false`, the display is centered to the
         * center of the new basemap.
         *
         * If try_preserve_pos is `true` (the default), the world location
         * (LatLon) currently displayed is preserved, if possible. Preserving
         * the current world position will succeed if that location is within
         * the new basemap.
         */
        void SetBaseMap(const std::shared_ptr<GeoDrawable> &new_map,
                        bool try_preserve_pos=true);

        /** Get the size of the map display area, in screen pixels. */
        const DisplayDeltaInt &GetDisplaySize() const { return m_display_size; }

        /** Set the size of the map display area, in screen pixels.
         *
         * This does not actively update the screen by itself,
         * `MapViewModel` has no knowledge of how or where the output is
         * shown. However, display engines may react if they see a new value
         * here.
         */
        void SetDisplaySize(const DisplayDeltaInt &new_size);

        /** Get the list of overlays drawn over the basemap.
         *
         * This can include other maps, GPS tracks, gridlines, ...
         */
        const OverlayList &GetOverlayList() const { return m_overlays; }

        /** Get the list of overlays drawn over the basemap. */
        void SetOverlayList(const OverlayList &overlay_list);

        /** A generation counter updated on every data modification.
         *
         * A change in the reported value indicates that the display should
         * likely be updated.
         */
        unsigned int GetChangeCtr() const { return m_change_ctr; }

    private:
        /** Try a location-preserving basemap change, fail if not possible. */
        bool TryChangeMapPreservePos(
                const std::shared_ptr<GeoDrawable> &new_map);

        /** Factor by how much the zoom is changed on one zoom 'step'. */
        static const double ZOOM_STEP;

        std::shared_ptr<GeoDrawable> m_base_map;
        OverlayList m_overlays;
        BaseMapCoord m_center;
        double m_zoom;
        DisplayDeltaInt m_display_size;
        unsigned int m_change_ctr;
};


/** A view object responsible for painting the current data model. */
class EXPORT MapView {
DISALLOW_COPY_AND_ASSIGN(MapView);
public:
    MapView(const std::shared_ptr<class Display> &display);

    /** Repaint the display based on data from a `MapViewModel`. */
    void Paint(const MapViewModel &mdm);

    /** Paint to a buffer based on data from a `MapViewModel`. */
    PixelBuf PaintToBuffer(ODMPixelFormat format,
                           const MapViewModel &mdm);

    /** Schedule a full repaint of the display. */
    void ForceFullRepaint();

private:
    static const int TILE_SIZE = 512;

    const std::shared_ptr<class Display> m_display;

    bool m_need_full_repaint;

    std::map<const TileCode, std::shared_ptr<class PixelPromise>
            > m_old_promise_cache, m_new_promise_cache;


    // IMPLEMENTATION FUNCTIONS
    ///////////////////////////

    /** Generate a `DisplayOrder` list for the current position and zoom.
     *
     * This handles the basemap as well as all overlays in a single function
     * call.
     *
     * Async (threaded) `PixelPromise` classes will be used if
     * `allow_async_promises` is `true`. This is generally a good thing, but
     * has to be disabled when rendering to a buffer, otherwise
     * not-yet-finished tiles will be drawn.
     */
    std::list<std::shared_ptr<class DisplayOrder>>
    GenerateDisplayOrders(const MapViewModel &mdm,
                          bool allow_async_promises);

    /** Generate display orders for a `GetRegion()` map layer, based on tiling.
     *
     * Calculate the map tiles required for filligng the display region,
     * and add display orders to show those tiles.
     * This is the default for most maps.
     *
     * Async (threaded) `PixelPromise` classes will be used if
     * `allow_async_promises` is `true`.
     */
    void PaintLayerTiled(
        const MapViewModel &mdm,
        std::list<std::shared_ptr<class DisplayOrder>> *orders,
        const std::shared_ptr<GeoDrawable> &map,
        const MapPixelCoordInt &base_pixel_topleft,
        const MapPixelCoordInt &base_pixel_botright,
        const MapPixelDeltaInt &tile_size,
        double transparency, bool allow_async_promises);

    /** Generate display orders for a `GetRegionDirect()` map layer.
     *
     * Add an order effecting GetRegionDirect(), which takes information
     * about the current projection, and returns a PixelBuf with the size
     * of the current display. That region is then shown directly on the
     * display without the need for rotation, stretching, ...
     * This is impractical for typical maps, as it requires re-reading the
     * image each time, it is however useful for displaying GPS Tracks and
     * other overlays. Moving overlays through the tile mechanism would
     * cause stretching, compromising display quality.
     */
    void PaintLayerDirect(
        const MapViewModel &mdm,
        std::list<std::shared_ptr<DisplayOrder>> *orders,
        const std::shared_ptr<GeoDrawable> &map,
        const DisplayDelta &disp_size_d,
        const MapPixelDelta &half_disp_size,
        double transparency);

    /** Get the map region of an overlay map required to fill the display area.
     *
     * Overlays may have any spatial relation to the base map, they can be
     * compressed, stretched, rotated, with transformation possibly varying
     * from point to point.
     *
     * Find out which region of the overlay map must be drawn so that the whole
     * display area is covered. The result is rounded to a full tile size and
     * returned in `overlay_tl` and `overlay_br`.
     *
     * Returning a rect may be suboptimal for oddly oriented maps, as it can
     * cause loading lots of tiles that do not actually end up on the screen.
     * It's good enough for now, though.
     */
    bool CalcOverlayRect(
        const std::shared_ptr<GeoDrawable> &base_map,
        const std::shared_ptr<GeoDrawable> &overlay_map,
        const MapPixelDeltaInt &tile_size,
        const MapPixelCoordInt &base_tl,
        const MapPixelCoordInt &base_br,
        MapPixelCoordInt *overlay_tl,
        MapPixelCoordInt *overlay_br);
};


// Utility functions

/** Get the basemap coordinate of an on-screen location. */
BaseMapCoord EXPORT BaseCoordFromDisplay(const DisplayCoord &disp,
                                         const MapViewModel &mdm);

/** Get the basemap coordinate of an on-screen location. */
BaseMapCoord EXPORT BaseCoordFromDisplay(const DisplayCoordCentered &disp,
                                         const MapViewModel &mdm);

/** Convert an on-screen coordinate delta to basemap coordinate space. */
BaseMapDelta EXPORT BaseDeltaFromDisplay(const DisplayDelta &disp,
                                         const MapViewModel &mdm);


/** Find the on-screen location of a map coordinate on an arbitrary map. */
DisplayCoordCentered EXPORT DisplayCoordCenteredFromMapPixel(
                        const MapPixelCoord &mpc,
                        const std::shared_ptr<GeoDrawable> &map,
                        const MapViewModel &mdm);

/** Convert an on-screen location to a map coordinate on an arbitrary map. */
DisplayCoordCentered EXPORT DisplayCoordCenteredFromMapPixel(
                        const MapPixelCoordInt &mpc,
                        const std::shared_ptr<GeoDrawable> &map,
                        const MapViewModel &mdm);

#endif

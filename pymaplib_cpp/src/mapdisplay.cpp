#include "mapdisplay.h"

#include <vector>
#include <list>
#include <cassert>
#include <limits>
#include <algorithm>

#include "rastermap.h"
#include "tiles.h"
#include "display.h"


static const int MAX_TILES = 100;
// ZOOM_STEP ** 4 == 2
const double MapDisplayManager::ZOOM_STEP =
                    1.189207115002721066717499970560475915;


MapDisplayManager::MapDisplayManager(
    const std::shared_ptr<class GeoDrawable> &initial_map,
    const DisplayDeltaInt &display_size)

    : m_base_map(initial_map), m_overlays(),
      m_center(BaseMapCoord(BaseMapDelta(m_base_map->GetSize() * 0.5))),
      m_zoom(1.0), m_display_size(display_size), m_change_ctr(0)
{ }

void MapDisplayManager::SetDisplaySize(const DisplayDeltaInt &new_size) {
    m_display_size = new_size;
    m_change_ctr++;
}

void MapDisplayManager::SetOverlayList(const OverlayList &overlay_list) {
    m_overlays = overlay_list;
    m_change_ctr++;
}

bool MapDisplayManager::TryChangeMapPreservePos(
        const std::shared_ptr<class GeoDrawable> &new_map)
{
    // Calc position of display center on earth
    LatLon point;
    if (!m_base_map->PixelToLatLon(m_center, &point))
        return false;

    // Convert that to map display coordinates on the new map
    MapPixelCoord new_center;
    if (!new_map->LatLonToPixel(point, &new_center))
        return false;

    // Check if old map center is within new map
    if (!new_center.IsInRect(MapPixelCoordInt(0, 0), new_map->GetSize()))
        return false;

    // Success in preserving the map position
    double new_zoom = m_zoom;
    double factor;
    if (!MetersPerPixel(new_map, new_center, &factor)) {
        return false;
    }
    new_zoom *= factor;

    if (!MetersPerPixel(m_base_map, m_center, &factor)) {
        return false;
    }
    new_zoom /= factor;

    m_zoom = new_zoom;
    m_center = BaseMapCoord(new_center);

    return true;
}

void MapDisplayManager::SetBaseMap(
        const std::shared_ptr<class GeoDrawable> &new_map,
        bool try_preserve_pos)
{
    if (new_map == m_base_map)
        return;

    bool preserve_pos = try_preserve_pos && TryChangeMapPreservePos(new_map);
    if (!preserve_pos) {
        m_center = BaseMapCoord(BaseMapDelta(new_map->GetSize() / 2.0));
        m_zoom = 1.0;
    }

    m_base_map = new_map;
    m_change_ctr++;
}

std::shared_ptr<class GeoDrawable> MapDisplayManager::GetBaseMap() const {
    return m_base_map;
}

double MapDisplayManager::GetZoom() const {
    return m_zoom;
}

void MapDisplayManager::SetCenter(const BaseMapCoord &center) {
    m_center = center;
    m_change_ctr++;
}

void MapDisplayManager::SetCenter(const LatLon &center) {
    BaseMapCoord new_center;
    m_base_map->LatLonToPixel(center, &new_center);
    SetCenter(new_center);
}

const BaseMapCoord &MapDisplayManager::GetCenter() const {
    return m_center;
}

void MapDisplayManager::StepZoom(double steps) {
    m_zoom *= pow(ZOOM_STEP, steps);
    m_change_ctr++;
}

DisplayCoordCentered CenteredCoordFromDisplay(const DisplayCoord& dc,
    const DisplayDeltaInt& disp)
{
    return DisplayCoordCentered(dc.x - disp.x / 2.0, dc.y - disp.y / 2.0);
}
DisplayCoord DisplayCoordFromCentered(const DisplayCoordCentered& dc,
    const DisplayDeltaInt& disp)
{
    return DisplayCoord(dc.x + disp.x / 2.0, dc.y + disp.y / 2.0);
}

void MapDisplayManager::StepZoom(double steps, const DisplayCoord &mouse_pos) {
    double m_zoom_before = m_zoom;
    DisplayCoordCentered old_pos = CenteredCoordFromDisplay(mouse_pos,
                                                            m_display_size);

    StepZoom(steps);

    DisplayCoordCentered new_pos = old_pos * m_zoom / m_zoom_before;
    MoveCenter(old_pos - new_pos);
}

void MapDisplayManager::SetZoomOneToOne() {
    m_zoom = 1.0;
    m_change_ctr++;
}

BaseMapCoord
MapDisplayManager::BaseCoordFromDisplay(const DisplayCoord &disp) const {
    return BaseCoordFromDisplay(CenteredCoordFromDisplay(disp, m_display_size));
}

BaseMapCoord
MapDisplayManager::BaseCoordFromDisplay(const DisplayCoordCentered &disp) const
{
    return m_center + BaseMapDelta(disp.x / m_zoom, disp.y / m_zoom);
}

BaseMapDelta
MapDisplayManager::BaseDeltaFromDisplay(const DisplayDelta &disp) const {
    return BaseMapDelta(disp.x / m_zoom, disp.y / m_zoom);
}

DisplayCoordCentered DisplayCoordCenteredFromBase(
    const BaseMapCoord &mpc, const MapDisplayManager &mdm)
{
    BaseMapDelta diff = mpc - mdm.GetCenter();
    return DisplayCoordCentered(diff.x * mdm.GetZoom(),
                                diff.y * mdm.GetZoom());
}

DisplayCoordCentered
MapDisplayManager::DisplayCoordCenteredFromMapPixel(
        const MapPixelCoord &mpc,
        const std::shared_ptr<class GeoDrawable> &map) const
{
    if (map == m_base_map) {
        return DisplayCoordCenteredFromBase(BaseMapCoord(mpc), *this);
    }

    LatLon world_pos;
    if (!map->PixelToLatLon(mpc, &world_pos)) {
        assert(false);
    }

    BaseMapCoord base_pos;
    if (!m_base_map->LatLonToPixel(world_pos, &base_pos)) {
        assert(false);
    }
    return DisplayCoordCenteredFromBase(base_pos, *this);
}

DisplayCoordCentered
MapDisplayManager::DisplayCoordCenteredFromMapPixel(
                        const MapPixelCoordInt &mpc,
                        const std::shared_ptr<class GeoDrawable> &map) const
{
    return DisplayCoordCenteredFromMapPixel(MapPixelCoord(mpc), map);
}

void MapDisplayManager::SetCenter(const DisplayCoord &center) {
    m_center = BaseCoordFromDisplay(center);
    m_change_ctr++;
}

void MapDisplayManager::MoveCenter(const DisplayDelta &disp_delta) {
    m_center = m_center - BaseDeltaFromDisplay(disp_delta);
    m_center.ClampToRect(MapPixelCoordInt(0,0),
                         MapPixelCoordInt(m_base_map->GetSize()));
    m_change_ctr++;
}



MapView::MapView(const std::shared_ptr<class Display> &display)
    : m_display(display), m_need_full_repaint(true),
      m_old_promise_cache(), m_new_promise_cache()
{}


void MapView::Paint(const MapDisplayManager &mdm) {
    if (mdm.GetDisplaySize() != m_display->GetDisplaySize()) {
        m_display->SetDisplaySize(mdm.GetDisplaySize());
        m_need_full_repaint = true;
    }

    if (m_need_full_repaint) {
        auto orders = GenerateDisplayOrders(mdm);
        m_display->Render(orders);
        m_need_full_repaint = false;
    } else {
        m_display->Redraw();
    }
}

PixelBuf MapView::PaintToBuffer(ODMPixelFormat format,
                                const MapDisplayManager &mdm)
{
    auto size = mdm.GetDisplaySize();
    auto orders = GenerateDisplayOrders(mdm);
    return m_display->RenderToBuffer(format, size.x, size.y, orders);
}


void MapView::ForceFullRepaint() {
    m_need_full_repaint = true;
    m_display->ForceRepaint();
}


std::list<std::shared_ptr<class DisplayOrder>>
MapView::GenerateDisplayOrders(const MapDisplayManager &mdm)
{
    std::list<std::shared_ptr<DisplayOrder>> orders;
    MapPixelDeltaInt tile_size(TILE_SIZE, TILE_SIZE);
    DisplayDelta half_disp_size_d(mdm.GetDisplaySize() / 2.0);
    MapPixelDelta half_disp_size(half_disp_size_d.x / mdm.GetZoom(),
                                 half_disp_size_d.y / mdm.GetZoom());

    MapPixelCoordInt base_pixel_tl(mdm.GetCenter() - half_disp_size);
    MapPixelCoordInt base_pixel_br(mdm.GetCenter() + half_disp_size);

    // We can't PaintLayerDirect() the base map, which is fine for now since we
    // only use Direct for overlays (e.g. GPS tracks).
    PaintLayerTiled(mdm, &orders, mdm.GetBaseMap(), base_pixel_tl, base_pixel_br,
        tile_size, 0.0);
    auto &overlays = mdm.GetOverlayList();
    for (auto ci = overlays.cbegin(); ci != overlays.cend(); ++ci) {
        if (!ci->GetEnabled()) {
            continue;
        }
        if (ci->GetMap()->SupportsDirectDrawing()) {
            PaintLayerDirect(mdm, &orders, ci->GetMap(),
                             DisplayDelta(mdm.GetDisplaySize()),
                             half_disp_size, ci->GetTransparency());
        }
        else {
            PaintLayerTiled(mdm, &orders, ci->GetMap(),
                base_pixel_tl, base_pixel_br,
                tile_size, ci->GetTransparency());
        }
    }
    m_old_promise_cache.clear();
    std::swap(m_old_promise_cache, m_new_promise_cache);
    return orders;
}

void MapView::PaintLayerDirect(
    const MapDisplayManager &mdm,
    std::list<std::shared_ptr<DisplayOrder>> *orders,
    const std::shared_ptr<class GeoDrawable> &map,
    const DisplayDelta &disp_size_d,
    const MapPixelDelta &half_disp_size,
    double transparency)
{
    const MapPixelCoord &base_pixel_tl = mdm.GetCenter() - half_disp_size;
    const MapPixelCoord &base_pixel_br = mdm.GetCenter() + half_disp_size;
    MapPixelDeltaInt disp_size_int = MapPixelDeltaInt(
        round_to_int(disp_size_d.x), round_to_int(disp_size_d.y));

    DisplayRectCentered rect(DisplayCoordCentered(-disp_size_int.x / 2.0,
        -disp_size_int.y / 2.0),
        DisplayDelta(disp_size_int.x, disp_size_int.y));
    auto promise = std::make_shared<PixelPromiseDirect>(
        map, disp_size_int, mdm.GetBaseMap(),
        base_pixel_tl, base_pixel_br);
    auto dorder = std::make_shared<DisplayOrder>(rect, transparency, promise);
    orders->push_back(dorder);
}

void MapView::PaintLayerTiled(
    const MapDisplayManager &mdm,
    std::list<std::shared_ptr<DisplayOrder>> *orders,
    const std::shared_ptr<class GeoDrawable> &map,
    const MapPixelCoordInt &base_pixel_topleft,
    const MapPixelCoordInt &base_pixel_botright,
    const MapPixelDeltaInt &tile_size,
    double transparency)
{
    MapPixelCoordInt tile_topleft, tile_botright;
    if (!CalcOverlayRect(mdm.GetBaseMap(), map, tile_size,
                         base_pixel_topleft, base_pixel_botright,
                         &tile_topleft, &tile_botright))
    {
        // Failed to paint overlay
        assert(false);
    }

    MapPixelDeltaInt tile_size_h(tile_size.x, 0);
    MapPixelDeltaInt tile_size_v(0, tile_size.y);

    for (int x = tile_topleft.x; x <= tile_botright.x; x += tile_size.x) {
        for (int y = tile_topleft.y; y <= tile_botright.y; y += tile_size.y) {
            MapPixelCoordInt map_pos(x, y);
            TileCode tilecode(map, map_pos, tile_size);

            DisplayCoordCentered disp_tl = mdm.DisplayCoordCenteredFromMapPixel(
                map_pos, map);
            DisplayCoordCentered disp_tr = mdm.DisplayCoordCenteredFromMapPixel(
                map_pos + tile_size_h, map);
            DisplayCoordCentered disp_bl = mdm.DisplayCoordCenteredFromMapPixel(
                map_pos + tile_size_v, map);
            DisplayCoordCentered disp_br = mdm.DisplayCoordCenteredFromMapPixel(
                map_pos + tile_size, map);
            DisplayRectCentered rect(disp_tl, disp_tr, disp_bl, disp_br);

            // Take an already created promise, if available.
            std::shared_ptr<PixelPromise> promise;
            auto old_promise = m_old_promise_cache.find(tilecode);
            if (old_promise != m_old_promise_cache.end()) {
                promise = old_promise->second;
            }
            else {
                if (map->SupportsConcurrentGetRegion()) {
                    // Load tiles on a background thread, if the map
                    // implementation can handle it.
                    //
                    // On completion, call ForceRepaint() in the BG thread.
                    // Use a weak_ptr to ensure correct behavior on shutdown.
                    //
                    // This relies rather delicately on the fact that
                    // ForceRepaint() resolves to a single call to
                    // InvalidateRect() on Windows, which is safe to run on
                    // any thread.
                    const std::weak_ptr<Display> &weak_display = m_display;
                    auto refresh = [weak_display]() {
                        if (auto display = weak_display.lock()) {
                            display->ForceRepaint();
                        }
                    };
                    promise = std::make_shared<PixelPromiseTiledAsync>(
                        tilecode, refresh);
                }
                else {
                    promise = std::make_shared<PixelPromiseTiled>(tilecode);
                }
            }
            auto dorder = std::make_shared<DisplayOrder>(rect, transparency,
                promise);
            orders->push_back(dorder);
            m_new_promise_cache[tilecode] = promise;
        }
    }
}

bool MapView::CalcOverlayRect(
    const std::shared_ptr<class GeoDrawable> &base_map,
    const std::shared_ptr<class GeoDrawable> &overlay_map,
    const MapPixelDeltaInt &tile_size,
    const MapPixelCoordInt &base_tl, const MapPixelCoordInt &base_br,
    MapPixelCoordInt *overlay_tl, MapPixelCoordInt *overlay_br)
{
    if (overlay_map == base_map) {
        *overlay_tl = MapPixelCoordInt(base_tl, tile_size.x);
        *overlay_br = MapPixelCoordInt(base_br, tile_size.x);
        return true;
    }
    LatLon point;
    MapPixelCoord overlay_point;
    long int x_min, x_max, y_min, y_max;
    x_min = y_min = std::numeric_limits<long int>::max();
    x_max = y_max = std::numeric_limits<long int>::min();

    // Iterate along the display border and find min/max overlay map pixels
    for (BorderIterator it(base_tl, base_br); !it.HasEnded(); ++it) {
        if (!base_map->PixelToLatLon(MapPixelCoord(*it), &point))
            return false;
        if (!overlay_map->LatLonToPixel(point, &overlay_point))
            return false;

        if (overlay_point.x < x_min) x_min = round_to_int(overlay_point.x);
        if (overlay_point.y < y_min) y_min = round_to_int(overlay_point.y);
        if (overlay_point.x > x_max) x_max = round_to_int(overlay_point.x);
        if (overlay_point.y > y_max) y_max = round_to_int(overlay_point.y);
    };

    // Create MapPixelCoords out of the minmax values, then round to tile_size.
    *overlay_tl = MapPixelCoordInt(MapPixelCoordInt(x_min, y_min),
                                   tile_size.x);
    *overlay_br = MapPixelCoordInt(MapPixelCoordInt(x_max, y_max),
                                   tile_size.y);
    return true;
}

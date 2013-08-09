#include "mapdisplay.h"

#include <vector>
#include <list>
#include <cassert>
#include <limits>

#include "rastermap.h"
#include "disp_ogl.h"
#include "tiles.h"
#include "disp_ogl.h"


static const int MAX_TILES = 100;
// ZOOM_STEP ** 4 == 2
const double MapDisplayManager::ZOOM_STEP =
                    1.189207115002721066717499970560475915;

MapDisplayManager::MapDisplayManager(
        std::shared_ptr<class DispOpenGL> &display,
        const class RasterMapCollection &maps)
    : m_display(display), m_maps(maps), m_base_map(&maps.Get(0)),
      m_center(BaseMapCoord(BaseMapDelta(m_base_map->GetSize() * 0.5))),
      m_zoom(1.0), m_overlays()
{ }

void MapDisplayManager::Resize(unsigned int width, unsigned int height) {
    m_display->Resize(width, height);
}

bool MapDisplayManager::TryChangeMapPreservePos(const RasterMap *new_map) {
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
    if (!MetersPerPixel(*new_map, new_center, &factor)) {
        return false;
    }
    new_zoom *= factor;

    if (!MetersPerPixel(*m_base_map, m_center, &factor)) {
        return false;
    }
    new_zoom /= factor;

    m_zoom = new_zoom;
    m_center = BaseMapCoord(new_center);

    return true;
}
void MapDisplayManager::ChangeMap(const RasterMap *new_map,
                                  bool try_preserve_pos)
{
    if (new_map == m_base_map)
        return;

    bool preserve_pos = try_preserve_pos && TryChangeMapPreservePos(new_map);
    if (!preserve_pos) {
        m_center = BaseMapCoord(BaseMapDelta(m_base_map->GetSize() / 2.0));
        m_zoom = 1.0;
    }

    m_base_map = new_map;
    m_overlays.clear();
    m_display->ForceRepaint();
}

void MapDisplayManager::AddOverlayMap(const RasterMap *new_map) {
    // Add new overlay or move existing overlay to last (topmost) position
    m_overlays.remove(new_map);
    m_overlays.push_back(new_map);
    m_display->ForceRepaint();
}

void MapDisplayManager::Paint() {
    DisplayDelta half_disp_size_d(m_display->GetDisplaySize() / m_zoom / 2.0);
    MapPixelDelta half_disp_size(half_disp_size_d.x, half_disp_size_d.y);

    MapPixelCoordInt tile_topleft(m_center - half_disp_size, TILE_SIZE);
    MapPixelCoordInt tile_botright(m_center + half_disp_size, TILE_SIZE);

    MapPixelDeltaInt tile_size(TILE_SIZE, TILE_SIZE);
    std::list<DisplayOrder> orders;
    PaintOneLayer(&orders, *m_base_map,
                  tile_topleft, tile_botright, tile_size);
    for (auto ci = m_overlays.cbegin(); ci != m_overlays.cend(); ++ci) {
        MapPixelCoordInt overlay_pixel_tl, overlay_pixel_br;
        if (!CalcOverlayTiles(*ci,
                              MapPixelCoordInt(m_center - half_disp_size),
                              MapPixelCoordInt(m_center + half_disp_size),
                              &overlay_pixel_tl, &overlay_pixel_br))
        {
            // Failed to paint overlay
            assert(false);
        }
        MapPixelCoordInt overlay_tiles_tl(overlay_pixel_tl, TILE_SIZE);
        MapPixelCoordInt overlay_tiles_br(overlay_pixel_br, TILE_SIZE);
        PaintOneLayer(&orders, **ci, overlay_tiles_tl, overlay_tiles_br,
                      tile_size, 0.5);
    }
    m_display->Render(orders);
}

bool MapDisplayManager::AdvanceAlongBorder(MapPixelCoordInt *base_point,
                                           const MapPixelCoordInt &base_tl,
                                           const MapPixelCoordInt &base_br)
{
    if (base_point->y == base_tl.y && base_point->x != base_br.x) {
        base_point->x++;  // top border
    }
    if (base_point->x == base_br.x && base_point->y != base_br.y) {
        base_point->y++;  // right border
    }
    if (base_point->y == base_br.y && base_point->x != base_tl.x) {
        base_point->x--;  // bottom border
    }
    if (base_point->x == base_tl.x && base_point->y != base_tl.y) {
        base_point->y--;  // right border
    }
    // Abort if we are back at the tl corner, otherwise continue
    if (*base_point == base_tl)
        return false;
    return true;
}

bool MapDisplayManager::CalcOverlayTiles(const RasterMap *overlay_map,
                                         const MapPixelCoordInt &base_tl,
                                         const MapPixelCoordInt &base_br,
                                         MapPixelCoordInt *overlay_tl,
                                         MapPixelCoordInt *overlay_br)
{
    LatLon point;
    MapPixelCoord overlay_point;
    long int x_min, x_max, y_min, y_max;
    x_min = y_min = std::numeric_limits<long int>::max();
    x_max = y_max = std::numeric_limits<long int>::min();

    // Iterate along the display border and find min/max overlay map pixels
    MapPixelCoordInt base_border = base_tl;
    do {
        if (!m_base_map->PixelToLatLon(MapPixelCoord(base_border), &point))
            return false;
        if (!overlay_map->LatLonToPixel(point, &overlay_point))
            return false;

        if (overlay_point.x < x_min) x_min = round_to_int(overlay_point.x);
        if (overlay_point.y < y_min) y_min = round_to_int(overlay_point.y);
        if (overlay_point.x > x_max) x_max = round_to_int(overlay_point.x);
        if (overlay_point.y > y_max) y_max = round_to_int(overlay_point.y);
    } while (AdvanceAlongBorder(&base_border, base_tl, base_br));

    *overlay_tl = MapPixelCoordInt(x_min, y_min);
    *overlay_br = MapPixelCoordInt(x_max, y_max);
    return true;
}

void MapDisplayManager::PaintOneLayer(std::list<class DisplayOrder> *orders,
                                      const class RasterMap &map,
                                      const MapPixelCoordInt &tile_topleft,
                                      const MapPixelCoordInt &tile_botright,
                                      const MapPixelDeltaInt &tile_size,
                                      double transparency)
{
    MapPixelDeltaInt tile_size_h(tile_size.x, 0);
    MapPixelDeltaInt tile_size_v(0, tile_size.y);

    for (int x = tile_topleft.x; x <= tile_botright.x; x += tile_size.x) {
        for (int y = tile_topleft.y; y <= tile_botright.y; y += tile_size.y) {
            MapPixelCoordInt map_pos(x, y);
            TileCode tilecode(map, map_pos, tile_size);

            DisplayCoordCentered disp_tl = DisplayCoordCenteredFromMapPixel(
                                           map_pos, map);
            DisplayCoordCentered disp_tr = DisplayCoordCenteredFromMapPixel(
                                           map_pos + tile_size_h, map);
            DisplayCoordCentered disp_bl = DisplayCoordCenteredFromMapPixel(
                                           map_pos + tile_size_v, map);
            DisplayCoordCentered disp_br = DisplayCoordCenteredFromMapPixel(
                                           map_pos + tile_size, map);

            orders->push_back(DisplayOrder(disp_tl, disp_tr, disp_bl, disp_br,
                                           tilecode, transparency));
        }
    }
}

const class RasterMap &MapDisplayManager::GetBaseMap() const {
    return *m_base_map;
}

double MapDisplayManager::GetZoom() const {
    return m_zoom;
}

double MapDisplayManager::GetCenterX() const {
    return m_center.x;
}

double MapDisplayManager::GetCenterY() const {
    return m_center.y;
}

const BaseMapCoord &MapDisplayManager::GetCenter() const {
    return m_center;
}

void MapDisplayManager::StepZoom(double steps) {
    m_zoom *= pow(ZOOM_STEP, steps);
    while (true) {
        // Don't allow to zoom out too far: approximate the number of tiles
        double num_tiles =
                m_display->GetDisplayWidth() / (m_zoom * TILE_SIZE) *
                m_display->GetDisplayHeight() / (m_zoom * TILE_SIZE);
        if (num_tiles > MAX_TILES) {
            m_zoom *= ZOOM_STEP;
        } else {
            break;
        }
    }
    m_display->ForceRepaint();
}

void MapDisplayManager::StepZoom(double steps, const DisplayCoord &mouse_pos) {
    double m_zoom_before = m_zoom;
    DisplayCoordCentered old_pos = CenteredCoordFromDisplay(mouse_pos,
                                                            *m_display);

    StepZoom(steps);

    DisplayCoordCentered new_pos = old_pos * m_zoom / m_zoom_before;
    DragMap(old_pos - new_pos);
}

void MapDisplayManager::SetZoomOneToOne() {
    m_zoom = 1.0;
    m_display->ForceRepaint();
}

BaseMapCoord
MapDisplayManager::BaseCoordFromDisplay(const DisplayCoord &disp) const {
    return BaseCoordFromDisplay(CenteredCoordFromDisplay(disp, *m_display));
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

DisplayCoordCentered
MapDisplayManager::DisplayCoordCenteredFromBase(const BaseMapCoord &mpc) const
{
    BaseMapDelta diff = mpc - m_center;
    return DisplayCoordCentered(diff.x * m_zoom, diff.y * m_zoom);
}

DisplayCoordCentered
MapDisplayManager::DisplayCoordCenteredFromMapPixel(const MapPixelCoord &mpc,
                                 const RasterMap &map) const
{
    if (&map == m_base_map)
        return DisplayCoordCenteredFromBase(BaseMapCoord(mpc));

    LatLon world_pos;
    if (!map.PixelToLatLon(mpc, &world_pos)) {
        assert(false);
    }

    BaseMapCoord base_pos;
    if (!m_base_map->LatLonToPixel(world_pos, &base_pos)) {
        assert(false);
    }
    return DisplayCoordCenteredFromBase(base_pos);
}

DisplayCoordCentered
MapDisplayManager::DisplayCoordCenteredFromMapPixel(
                        const MapPixelCoordInt &mpc,
                        const RasterMap &map) const
{
    return DisplayCoordCenteredFromMapPixel(MapPixelCoord(mpc), map);
}

void MapDisplayManager::CenterToDisplayCoord(const DisplayCoord &center) {
    m_center = BaseCoordFromDisplay(center);
    m_display->ForceRepaint();
}

void MapDisplayManager::DragMap(const DisplayDelta &disp_delta) {
    m_center = m_center - BaseDeltaFromDisplay(disp_delta);
    m_center.ClampToRect(MapPixelCoordInt(0,0),
                         MapPixelCoordInt(m_base_map->GetSize()));
    m_display->ForceRepaint();
}

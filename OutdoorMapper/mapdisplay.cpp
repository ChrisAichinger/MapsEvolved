#include <vector>
#include <list>

#include "rastermap.h"
#include "disp_ogl.h"
#include "mapdisplay.h"
#include "tiles.h"


static const int MAX_TILES = 100;
// ZOOM_STEP ** 4 == 2
const double MapDisplayManager::ZOOM_STEP =
                    1.189207115002721066717499970560475915;

MapDisplayManager::MapDisplayManager(
        std::shared_ptr<class DispOpenGL> &display,
        const class RasterMapCollection &maps)
    : m_display(display), m_maps(maps), m_base_map(&maps.Get(0)),
      m_center(m_base_map->GetSize() * 0.5),
      m_zoom(1.0)
{ }

void MapDisplayManager::Resize(unsigned int width, unsigned int height) {
    m_display->Resize(width, height);
}

void MapDisplayManager::ChangeMap(const RasterMap *new_map,
                                  bool try_preserve_pos)
{
    if (new_map == m_base_map)
        return;

    if (try_preserve_pos) {
        LatLon point = m_base_map->PixelToLatLon(m_center);
        MapPixelCoord new_center = new_map->LatLonToPixel(point);
        // Check if old map position is within new map
        if (new_center.IsInRect(MapPixelCoord(0, 0), new_map->GetSize())) {
            m_zoom *= MetersPerPixel(new_map, new_center);
            m_zoom /= MetersPerPixel(m_base_map, m_center);
            m_center = new_center;
        } else {
            try_preserve_pos = false;
        }
    }

    if (!try_preserve_pos) {
        m_center = MapPixelCoord(m_base_map->GetSize() / 2);
        m_zoom = 1.0;
    }

    m_base_map = new_map;
    m_display->ForceRepaint();
}

void MapDisplayManager::Paint() {
    MapPixelDelta half_disp_size(m_display->GetDisplaySize() / 2.0, *this);

    MapPixelCoordInt tile_topleft(m_center - half_disp_size, TILE_SIZE);
    MapPixelCoordInt tile_botright(m_center + half_disp_size, TILE_SIZE);

    std::list<DisplayOrder> orders;
    for (int tx = tile_topleft.x; tx <= tile_botright.x; tx += TILE_SIZE) {
        for (int ty = tile_topleft.y; ty <= tile_botright.y; ty += TILE_SIZE) {
            MapPixelCoordInt map_pos(tx, ty);
            TileCode tilecode(*m_base_map, map_pos, TILE_SIZE);

            DisplayCoordCentered disp_pos(map_pos, *this);
            DisplayDelta disp_size(TILE_SIZE * m_zoom, TILE_SIZE * m_zoom);
            orders.push_back(DisplayOrder(disp_pos, disp_size, tilecode));
        }
    }
    m_display->Render(orders);
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

const MapPixelCoord &MapDisplayManager::GetCenter() const {
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
    DisplayCoordCentered old_pos(mouse_pos, *m_display);

    StepZoom(steps);

    DisplayCoordCentered new_pos = old_pos * m_zoom / m_zoom_before;
    DragMap(old_pos - new_pos);
}

MapPixelCoord
MapDisplayManager::MapPixelCoordFromDisplay(const DisplayCoord &disp) const {
    DisplayCoordCentered centered(disp, *m_display);
    return MapPixelCoord(centered, *this);
}

void MapDisplayManager::CenterToDisplayCoord(const DisplayCoord &center) {
    m_center = MapPixelCoordFromDisplay(center);
    m_display->ForceRepaint();
}

void MapDisplayManager::DragMap(const DisplayDelta &disp_delta) {
    m_center = m_center - MapPixelDelta(disp_delta, *this);
    m_center.ClampToRect(MapPixelCoord(0,0),
                         MapPixelCoord(m_base_map->GetSize()));
    m_display->ForceRepaint();
}

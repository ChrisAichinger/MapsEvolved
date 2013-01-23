#include <vector>

#include "rastermap.h"
#include "disp_ogl.h"
#include "mapdisplay.h"
#include "tiles.h"

static const int MAX_TILES = 100;
// ZOOM_STEP ** 4 == 2
const double MapDisplayManager::ZOOM_STEP =
                    1.189207115002721066717499970560475915;

MapDisplayManager::MapDisplayManager(std::shared_ptr<class DispOpenGL> &display,
                                     const class RasterMapCollection &maps) 
    : m_display(display), m_maps(maps), m_base_map(&maps.Get(0)),
      m_center_x(m_base_map->GetWidth() * 0.5f),
      m_center_y(m_base_map->GetHeight() * 0.5f),
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
        double x = m_center_x;
        double y = m_center_y;
        m_base_map->PixelToLatLong(&x, &y);
        new_map->LatLongToPixel(&x, &y);
        // Check if old map position is within new map
        if (IsInRect(x, y, new_map->GetWidth(), new_map->GetHeight())) {
            m_zoom *= MetersPerPixel(new_map, x, y) / MetersPerPixel(m_base_map, m_center_x, m_center_y);
            m_center_x = x;
            m_center_y = y;
        } else {
            try_preserve_pos = false;
        }
    }

    if (!try_preserve_pos) {
        m_center_x = m_base_map->GetWidth() * 0.5f;
        m_center_y = m_base_map->GetHeight() * 0.5f;
        m_zoom = 1.0;
    }

    m_base_map = new_map;
    m_display->ForceRepaint();
}

void MapDisplayManager::Paint() {
    unsigned int d_width = m_display->GetDisplayWidth();
    unsigned int d_height = m_display->GetDisplayHeight();
    double bm_half_width = d_width / m_zoom * 0.5;
    double bm_half_height = d_height / m_zoom * 0.5;

    double bm_left = m_center_x - bm_half_width;
    double bm_right = m_center_x + bm_half_width;
    double bm_top = m_center_y - bm_half_height;
    double bm_bottom = m_center_y + bm_half_height;

    // All values inclusive, first...last (not last+1 tile)
    int tiles_left = round_to_neg_inf(bm_left, TILE_SIZE);
    int tiles_top = round_to_neg_inf(bm_top, TILE_SIZE);
    int tiles_right = round_to_neg_inf(bm_right, TILE_SIZE);
    int tiles_bottom = round_to_neg_inf(bm_bottom, TILE_SIZE);

    int num_tiles = ((tiles_right - tiles_left) / TILE_SIZE + 1) *
                    ((tiles_bottom - tiles_top) / TILE_SIZE + 1);
    std::vector<DisplayOrder> orders;
    orders.reserve(num_tiles);
    for (int tx = tiles_left; tx <= tiles_right; tx += TILE_SIZE) {
        for (int ty = tiles_top; ty <= tiles_bottom; ty += TILE_SIZE) {
            double x = (tx - m_center_x) * m_zoom;
            double y = (ty - m_center_y) * m_zoom;
            double width = TILE_SIZE * m_zoom;
            double height = TILE_SIZE * m_zoom;
            double xe = x + width;
            double ye = y + height;
            TileCode tilecode(*m_base_map, tx, ty, TILE_SIZE);
            orders.push_back(DisplayOrder(x, y, width, height, tilecode));
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

void MapDisplayManager::StepZoom(int steps) {
    if (steps > 0) {
        for (int i = 0; i < steps; i++) {
            m_zoom *= ZOOM_STEP;
        }
    } else {
        for (int i = 0; i > steps; i--) {
            m_zoom /= ZOOM_STEP;

            // Don't allow to zoom out too far: approximate the number of tiles
            int num_tiles = static_cast<int>(
                    m_display->GetDisplayWidth() / (m_zoom * TILE_SIZE) *
                    m_display->GetDisplayHeight() / (m_zoom * TILE_SIZE));
            if (num_tiles > MAX_TILES) {
                m_zoom *= ZOOM_STEP;
            }
        }
    }
    m_display->ForceRepaint();
}

void MapDisplayManager::StepZoom(int steps, int mouse_x, int mouse_y) {
    double m_zoom_before = m_zoom;
    double rel_x = mouse_x - m_display->GetDisplayWidth() / 2.0f;
    double rel_y = mouse_y - m_display->GetDisplayHeight() / 2.0f;

    StepZoom(steps);

    double new_x = rel_x * m_zoom / m_zoom_before;
    double new_y = rel_y * m_zoom / m_zoom_before;

    DragMap(round_to_int(rel_x - new_x), round_to_int(rel_y - new_y));
}

void MapDisplayManager::CenterToDisplayCoord(int center_x, int center_y) {
    Point<int> disp(center_x, center_y);
    Point<double> base = BaseCoordFromDisplayCoord(disp);
    m_center_x = base.GetX();
    m_center_y = base.GetY();
    m_display->ForceRepaint();
}

void MapDisplayManager::DragMap(int dx, int dy) {
    double max_x = m_base_map->GetWidth() - 1.0f;
    double max_y = m_base_map->GetHeight() - 1.0f;
    m_center_x = ValueBetween(0.0, m_center_x - dx / m_zoom, max_x);
    m_center_y = ValueBetween(0.0, m_center_y - dy / m_zoom, max_y);
    m_display->ForceRepaint();
}

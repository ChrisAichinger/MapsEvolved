#include <vector>

#include "rastermap.h"
#include "disp_ogl.h"
#include "mapdisplay.h"
#include "tiles.h"

static const int MAX_TILES = 100;

MapDisplayManager::MapDisplayManager(class DispOpenGL &display,
                                     const class RasterMapCollection &maps) 
    : m_display(display), m_maps(maps), m_base_map(maps.GetDefaultMap()),
      m_center_x(m_base_map.GetWidth() * 0.5f), m_center_y(m_base_map.GetHeight() * 0.5f),
      m_zoom(1.0)
{ }

void MapDisplayManager::Paint() {
    unsigned int d_width = m_display.GetDisplayWidth();
    unsigned int d_height = m_display.GetDisplayHeight();
    double bm_width = d_width / m_zoom;
    double bm_height = d_height / m_zoom;

    double bm_left = m_center_x - (bm_width / 2);
    double bm_right = m_center_x + (bm_width / 2);
    double bm_top = m_center_y - (bm_height / 2);
    double bm_bottom = m_center_y + (bm_height / 2);

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
            TileCode tilecode(m_base_map, tx, ty, TILE_SIZE);
            orders.push_back(DisplayOrder(x, y, width, height, tilecode));
        }
    }
    m_display.Render(orders);
}

const class RasterMap &MapDisplayManager::GetBaseMap() const {
    return m_base_map;
}

void MapDisplayManager::StepZoom(int steps) {
    if (steps > 0) {
        for (int i = 0; i < steps; i++) {
            m_zoom *= 1.2f;
        }
    } else {
        for (int i = 0; i > steps; i--) {
            m_zoom /= 1.2f;

            // Don't allow to zoom out too far: approximate the number of tiles
            int num_tiles = static_cast<int>(
                    m_display.GetDisplayWidth() / (m_zoom * TILE_SIZE) *
                    m_display.GetDisplayHeight() / (m_zoom * TILE_SIZE));
            if (num_tiles > MAX_TILES) {
                m_zoom *= 1.2f;
            }
        }
    }
}

void MapDisplayManager::StepZoom(int steps, int mouse_x, int mouse_y) {
    double m_zoom_before = m_zoom;
    double rel_x = mouse_x - m_display.GetDisplayWidth() / 2.0f;
    double rel_y = mouse_y - m_display.GetDisplayHeight() / 2.0f;

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
}

void MapDisplayManager::DragMap(int dx, int dy) {
    double max_x = m_base_map.GetWidth() - 1.0f;
    double max_y = m_base_map.GetHeight() - 1.0f;
    m_center_x = ValueBetween(0.0, m_center_x - dx / m_zoom, max_x);
    m_center_y = ValueBetween(0.0, m_center_y - dy / m_zoom, max_y);
}

#ifndef ODM__MAPDISPLAY_H
#define ODM__MAPDISPLAY_H

#include "util.h"

template <typename T>
class Point {
    public:
        Point() : m_x(0), m_y(0) { };
        Point(T x, T y) : m_x(x), m_y(y) { };

        T GetX() const { return m_x; };
        T GetY() const { return m_y; };

        T SetX(const T val) { return m_x = val; };
        T SetY(const T val) { return m_y = val; };
        void SetXY(T x, T y) { m_x = x; m_y = y; };
    private:
        T m_x, m_y;
};

class MapDisplayManager {
    public:
        MapDisplayManager(std::shared_ptr<class DispOpenGL> &display,
                          const class RasterMapCollection &maps);
        const class RasterMap &GetBaseMap() const;

        void Resize(unsigned int width, unsigned int height);
        void ChangeMap(const RasterMap *new_map, bool try_preserve_pos=true);

        double GetZoom() const;
        void StepZoom(int steps);
        // mouse_x, mouse_y are mouse coordinates relative to the map panel
        // The map location under the mouse is held constant
        void StepZoom(int steps, int mouse_x, int mouse_y);
        void DragMap(int dx, int dy);
        void CenterToDisplayCoord(int center_x, int center_y);
        void Paint();

        template <typename T>
        Point<double>
        DisplayCoordFromBaseCoord(const class Point<T> &base_p) {
            double rel_x = (base_p.GetX() - m_center_x) * m_zoom;
            double rel_y = (base_p.GetY() - m_center_y) * m_zoom;
            return Point<double>(rel_x + m_display->GetDisplayWidth() / 2.0,
                                 rel_y + m_display->GetDisplayHeight() / 2.0);
        };

        template <typename T>
        Point<double>
        BaseCoordFromDisplayCoord(const class Point<T> &disp_p) {
            double rel_x = disp_p.GetX() - m_display->GetDisplayWidth() / 2.0;
            double rel_y = disp_p.GetY() - m_display->GetDisplayHeight() / 2.0;
            return Point<double>(m_center_x + rel_x / m_zoom,
                                 m_center_y + rel_y / m_zoom);
        };

    private:
        DISALLOW_COPY_AND_ASSIGN(MapDisplayManager);

        static const int TILE_SIZE = 512;
        static const double ZOOM_STEP;

        std::shared_ptr<class DispOpenGL> m_display;
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_base_map;

        double m_center_x, m_center_y;
        double m_zoom;
};

#endif

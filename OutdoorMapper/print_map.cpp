#include "print_map.h"

#include "util.h"
#include "mapdisplay.h"
#include "projection.h"
#include "rastermap.h"



MapPrinter::MapPrinter(std::shared_ptr<class MapDisplayManager> &display,
                       double scale_factor)
    : m_display(display), m_scale(scale_factor)
{}


bool MapPrinter::Print(HDC hdc) {
    int print_width_mm = GetDeviceCaps(hdc, HORZSIZE);
    int print_height_mm = GetDeviceCaps(hdc, VERTSIZE);
    int horzres = GetDeviceCaps(hdc, HORZRES);
    int vertres = GetDeviceCaps(hdc, VERTRES);
    //double dpi_x = INCH_to_MM * horzres / horzsize;
    //double dpi_y = INCH_to_MM * vertres / vertsize;

    double map_width_m = print_width_mm * m_scale / 1000.0;
    double map_height_m = print_width_mm * m_scale / 1000.0;

    const RasterMap &map = m_display->GetBaseMap();
    const MapPixelCoord &map_center = m_display->GetCenter();
    int dx, dy;
    for (dx=1; true; dx++) {
        double dist;
        GetMapDistance(map, map_center, dx, 0, &dist);
        if (dist < map_width_m)
            break;
    }
    for (dy=1; true; dy++) {
        double dist;
        GetMapDistance(map, map_center, 0, dy, &dist);
        if (dist < map_width_m)
            break;
    }
    dx = dy = 20000;
    MapPixelCoordInt center(map_center);
    auto pixels = map.GetRegion(center, MapPixelDeltaInt(2*dx, 2*dy));

    BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER),
                         2*dx, 2*dy, 1, 32, BI_RGB, 0, 0, 0, 0, 0 }, 0 };
    int res = StretchDIBits(hdc, 0, 0, horzres, vertres,
                                 0, 0, 2*dx, 2*dy,
                                 pixels.GetRawData(), &bmi, 0, SRCCOPY);
    if (res == 0 || res == GDI_ERROR) {
        throw std::runtime_error("Failed to copy pixels to HDC");
    }

    return false;
}

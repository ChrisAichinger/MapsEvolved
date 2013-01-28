#include <Windows.h>

#include "util.h"
#include "print_map.h"
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

    int Cx = round_to_int(m_display->GetCenterX());
    int Cy = round_to_int(m_display->GetCenterY());
    const RasterMap &map = m_display->GetBaseMap();

    int dx, dy;
    for (dx=1; GetMapDistance(map, Cx, Cy, dx, 0) < map_width_m; dx++);
    for (dy=1; GetMapDistance(map, Cx, Cy, 0, dy) < map_height_m; dy++);
    auto pixels = map.GetRegion(Cx - dx, Cy - dy, 2*dx, 2*dy);

    BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER),
                         2*dx, 2*dy, 1, 32, BI_RGB, 0, 0, 0, 0, 0 }, 0 };
    int res = StretchDIBits(hdc, 0, 0, horzres, vertres, 
                                 0, 0, 2*dx, 2*dy,
                                 pixels.get(), &bmi, 0, SRCCOPY);
    if (res == 0 || res == GDI_ERROR) {
        throw std::runtime_error("Failed to copy pixels to HDC");
    }

    return false;
}

// Rax - Gr. Priel 2441 x 15 pixels
// Gr. Priel 47.716944°, 14.063056°
// Rax 47.689444°, 15.687778°
//
// distance (m)  meters/pixel
// 121975.354    49.9694199
// 121975        49,9692749
// 121725        49.8668578

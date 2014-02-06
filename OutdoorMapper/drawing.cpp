#include "drawing.h"

#include <algorithm>

void ClippedSetPixel(unsigned int* dest, const MapPixelDeltaInt &size,
                     int xx, int yy, unsigned int val)
{
    if ((xx) >= 0 && (yy) >= 0 && (xx) < (size).x && (yy) < (size).y) {
        dest[(xx) + size.x * (size.y - (yy) - 1)] = (val);
    }

}

void ClippedLine(unsigned int* dest, MapPixelDeltaInt size,
                 const MapPixelCoordInt &start, const MapPixelCoordInt &end,
                 const unsigned int color)
{
    int x1 = start.x;
    int x2 = end.x;
    int y1 = start.y;
    int y2 = end.y;

    // Bresenham's line algorithm
    const bool is_steep = (abs(y2 - y1) > abs(x2 - x1));
    if (is_steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }

    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    const int dx = x2 - x1;
    const int dy = abs(y2 - y1);
    int error = dx;
    const int ystep = (y1 < y2) ? 1 : -1;
    int y = y1;

    for (int x = x1; x < x2; x++) {
        if (is_steep) {
            ClippedSetPixel(dest, size, y, x, color);
        } else {
            ClippedSetPixel(dest, size, x, y, color);
        }

        error -= 2 * dy;
        if (error < 0) {
            y += ystep;
            error += 2 * dx;
        }
    }
}



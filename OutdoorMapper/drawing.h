#ifndef ODM__DRAWING_H
#define ODM__DRAWING_H

#include "coordinates.h"

void ClippedSetPixel(unsigned int* dest, const MapPixelDeltaInt &size,
                     int xx, int yy, unsigned int val);
void ClippedLine(unsigned int* dest, MapPixelDeltaInt size,
                 const MapPixelCoordInt &start, const MapPixelCoordInt &end,
                 const unsigned int color);

#endif

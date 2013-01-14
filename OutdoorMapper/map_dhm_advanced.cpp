#define _USE_MATH_DEFINES
#include <memory>
#include <cassert>

#include "util.h"
#include "bezier.h"
#include "map_dhm_advanced.h"

GradientMap::GradientMap(std::shared_ptr<const RasterMap> orig_map) 
    : m_orig_map(orig_map)
{
    assert(orig_map->GetType() == RasterMap::TYPE_DHM);
}

RasterMap::RasterMapType GradientMap::GetType() const {
    return RasterMap::TYPE_GRADIENT;
}
unsigned int GradientMap::GetWidth() const {
    return m_orig_map->GetWidth();
}
unsigned int GradientMap::GetHeight() const {
    return m_orig_map->GetHeight();
}
void GradientMap::PixelToPCS(double *x, double *y) const {
    return m_orig_map->PixelToPCS(x, y);
}
void GradientMap::PCSToPixel(double *x, double *y) const {
    return m_orig_map->PCSToPixel(x, y);
}
const class Projection &GradientMap::GetProj() const {
    return m_orig_map->GetProj();
}
void GradientMap::PixelToLatLong(double *x, double *y) const {
    return m_orig_map->PixelToLatLong(x, y);
}
void GradientMap::LatLongToPixel(double *x, double *y) const {
    return m_orig_map->LatLongToPixel(x, y);
}
const std::wstring &GradientMap::GetFname() const {
    return m_orig_map->GetFname();
}

static std::vector<unsigned int> TimeCounter;


#include <Windows.h>
#define DEST(xx,yy) dest[(xx) + width * (yy)]
#define SRC(xx,yy) src[(xx) + width * (yy)]
std::shared_ptr<unsigned int> GradientMap::GetRegion(
        int x, int y, unsigned int width, unsigned int height) const
{
    auto orig_data = m_orig_map->GetRegion(x, y, width, height);
    //auto orig_data = LoadBufferFromBMP(L"example.bmp");
    std::shared_ptr<unsigned int> data(new unsigned int[width * height],
                                       ArrayDeleter<unsigned int>());

    unsigned int *src = orig_data.get();
    unsigned int *dest = data.get();
    DWORD tmStart = timeGetTime();
    for (unsigned int x=0; x < width; x++) {
        for (unsigned int y=0; y < height; y++) {
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1) {
                DEST(x, y) = 0;
                continue;
            }

            MapBezier bezier(src, width, height, x, y);

            double xa = 0.5;
            double ya = 0.5;
            bezier.GetGradient(&xa, &ya);
            DEST(x, y) = static_cast<unsigned int>(HSV_to_RGB(
                          (unsigned char)(255*240/360 - SRC(x,y)*255/4000),
                          255,
                          (unsigned char)min(255, 128 + 1.5*(xa-ya))));
        }
    }
    DWORD diff = timeGetTime() - tmStart;
    TimeCounter.push_back(diff);
    return data;
}
#undef DEST
#undef SRC


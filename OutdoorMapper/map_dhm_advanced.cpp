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
MapPixelDelta GradientMap::GetSize() const {
    return m_orig_map->GetSize();
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
LatLon GradientMap::PixelToLatLon(const MapPixelCoord &pos) const {
    return m_orig_map->PixelToLatLon(pos);
}
MapPixelCoord GradientMap::LatLonToPixel(const LatLon &pos) const {
    return m_orig_map->LatLonToPixel(pos);
}
const std::wstring &GradientMap::GetFname() const {
    return m_orig_map->GetFname();
}

static std::vector<unsigned int> TimeCounter;


#include <Windows.h>
#define DEST(xx,yy) dest[(xx) + size.x * (yy)]
#define SRC(xx,yy) src[(xx) + req_size.x * (yy)]
std::shared_ptr<unsigned int> GradientMap::GetRegion(
        const MapPixelCoordInt &pos, const MapPixelDeltaInt &size) const
{
    MapPixelCoordInt req_pos = pos - MapPixelDeltaInt(1, 1);
    MapPixelDeltaInt req_size = size + MapPixelDeltaInt(2, 2);
    auto orig_data = m_orig_map->GetRegion(req_pos, req_size);
    //auto orig_data = LoadBufferFromBMP(L"example.bmp");
    std::shared_ptr<unsigned int> data(new unsigned int[size.x * size.y],
                                       ArrayDeleter<unsigned int>());

    unsigned int *src = orig_data.get();
    unsigned int *dest = data.get();
    DWORD tmStart = timeGetTime();
    for (int x=0; x < size.x; x++) {
        for (int y=0; y < size.y; y++) {
            unsigned int elevation = SRC(x+1, y+1);
            MapBezier bez(src, MapPixelCoordInt(x+1, y+1), req_size);
            MapBezierGradient grad = bez.GetGradient(bez.GetCreationPos());

            DEST(x, y) = HSV_to_RGB(
                      255*240/360 - elevation*255/4000,
                      255,
                      ValueBetween(0,
                                   static_cast<int>(128+1.25*(grad.x-grad.y)),
                                   255));
        }
    }
    DWORD diff = timeGetTime() - tmStart;
    TimeCounter.push_back(diff);
    return data;
}
#undef DEST
#undef SRC


#include "map_dhm_advanced.h"

#define _USE_MATH_DEFINES
#include <memory>
#include <cassert>
#include <math.h>

#include "util.h"
#include "bezier.h"

GradientMap::GradientMap(const std::shared_ptr<RasterMap> &orig_map)
    : m_orig_map(orig_map)
{
    assert(orig_map->GetType() == RasterMap::TYPE_DHM);
}

GeoDrawable::DrawableType GradientMap::GetType() const {
    return RasterMap::TYPE_GRADIENT_MAP;
}
unsigned int GradientMap::GetWidth() const {
    return m_orig_map->GetWidth();
}
unsigned int GradientMap::GetHeight() const {
    return m_orig_map->GetHeight();
}
MapPixelDeltaInt GradientMap::GetSize() const {
    return m_orig_map->GetSize();
}
Projection GradientMap::GetProj() const {
    return m_orig_map->GetProj();
}
bool GradientMap::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const
{
    return m_orig_map->PixelToLatLon(pos, result);
}
bool GradientMap::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const
{
    return m_orig_map->LatLonToPixel(pos, result);
}
const std::wstring &GradientMap::GetFname() const {
    return m_orig_map->GetFname();
}


static TimeCounter time_counter;
static TimeCounter time_counter_loaddisc;

#define DEST(xx,yy) dest[(xx) + size.x * (yy)]
#define SRC(xx,yy) src[(xx) + req_size.x * (yy)]
PixelBuf GradientMap::GetRegion(
        const MapPixelCoordInt &pos, const MapPixelDeltaInt &size) const
{
    time_counter_loaddisc.Start();
    MapPixelCoordInt req_pos = pos - MapPixelDeltaInt(1, 1);
    MapPixelDeltaInt req_size = size + MapPixelDeltaInt(2, 2);
    auto orig_data = m_orig_map->GetRegion(req_pos, req_size);
    //auto orig_data = LoadBufferFromBMP(L"example.bmp");
    std::shared_ptr<unsigned int> data(new unsigned int[size.x * size.y],
                                       ArrayDeleter<unsigned int>());

    unsigned int *src = orig_data.GetRawData();
    unsigned int *dest = data.get();
    time_counter_loaddisc.Stop();
    time_counter.Start();
    for (int x=0; x < size.x; x++) {
        for (int y=0; y < size.y; y++) {
            unsigned int elevation = SRC(x+1, y+1);
            MapPixelCoordInt pos(x+1, y+1);
            MapBezierGradient grad = Fast3x3CenterGradient(src, pos, req_size);

            DEST(x, y) = HSV_to_RGB(
                      255*240/360 - elevation*255/4000,
                      255,
                      ValueBetween(0,
                                   static_cast<int>(128+1.25*(grad.x-grad.y)),
                                   255));
        }
    }
    time_counter.Stop();
    return PixelBuf(data, size.x, size.y);
}
#undef DEST
#undef SRC


SteepnessMap::SteepnessMap(const std::shared_ptr<RasterMap> &orig_map)
    : m_orig_map(orig_map)
{
    assert(orig_map->GetType() == RasterMap::TYPE_DHM);
}

GeoDrawable::DrawableType SteepnessMap::GetType() const {
    return RasterMap::TYPE_STEEPNESS_MAP;
}
unsigned int SteepnessMap::GetWidth() const {
    return m_orig_map->GetWidth();
}
unsigned int SteepnessMap::GetHeight() const {
    return m_orig_map->GetHeight();
}
MapPixelDeltaInt SteepnessMap::GetSize() const {
    return m_orig_map->GetSize();
}
Projection SteepnessMap::GetProj() const {
    return m_orig_map->GetProj();
}
bool SteepnessMap::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const
{
    return m_orig_map->PixelToLatLon(pos, result);
}
bool SteepnessMap::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const
{
    return m_orig_map->LatLonToPixel(pos, result);
}
const std::wstring &SteepnessMap::GetFname() const {
    return m_orig_map->GetFname();
}

static unsigned int steepness_colors[] = {
    0xffffff, 0xedffed, 0x95fd95, 0x63f563,
    0x00e600, 0x00dca2, 0x009ff6, 0x0078ff,
    0x0019ff, 0x0007e5, 0x1700b3, 0x5a00b0,
    0x80009b, 0x66006d, 0x5d475d, 0x5d5d5d,
    0x3f3f3f, 0x272727, 0x000000,
};

#define DEST(xx,yy) dest[(xx) + size.x * (yy)]
#define SRC(xx,yy) src[(xx) + req_size.x * (yy)]
PixelBuf SteepnessMap::GetRegion(
        const MapPixelCoordInt &pos, const MapPixelDeltaInt &size) const
{
    double mpp;
    if (!MetersPerPixel(m_orig_map, pos + size/2, &mpp)) {
        // Return zero-initialized memory block.
        return PixelBuf(size.x, size.y);
    }
    unsigned int bezier_pixels = Bezier::N_POINTS - 1;
    double inv_bezier_meters = 1 / (bezier_pixels * mpp);

    MapPixelCoordInt req_pos = pos - MapPixelDeltaInt(1, 1);
    MapPixelDeltaInt req_size = size + MapPixelDeltaInt(2, 2);
    auto orig_data = m_orig_map->GetRegion(req_pos, req_size);
    std::shared_ptr<unsigned int> data(new unsigned int[size.x * size.y],
                                       ArrayDeleter<unsigned int>());

    unsigned int *src = orig_data.GetRawData();
    unsigned int *dest = data.get();
    for (int x=0; x < size.x; x++) {
        for (int y=0; y < size.y; y++) {
            MapPixelCoordInt pos(x+1, y+1);
            MapBezierGradient grad = Fast3x3CenterGradient(src, pos, req_size);
            grad *= inv_bezier_meters;
            double grad_steepness = atan(grad.Abs());
            int color_index = static_cast<int>(grad_steepness / (M_PI / 2) * 18);
            DEST(x, y) = steepness_colors[color_index];
        }
    }
    return PixelBuf(data, size.x, size.y);
}
#undef DEST
#undef SRC

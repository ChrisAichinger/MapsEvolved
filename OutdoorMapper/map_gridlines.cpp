#include "map_gridlines.h"

#include <limits>
#include <algorithm>
#include <tuple>
#include <cassert>
#define _USE_MATH_DEFINES
#include <math.h>

#include "drawing.h"


std::wstring Gridlines::fname = L"";
static const int PIXELS_PER_DEGREE = 100;

Gridlines::Gridlines()
    // 360 dregrees x (longitude), 180 degrees y (latitude, -90 to +90)
    : m_size(360 * PIXELS_PER_DEGREE, 180 * PIXELS_PER_DEGREE)
{}

Gridlines::~Gridlines() {}

MapRegion
Gridlines::GetRegion(const MapPixelCoordInt &pos,
                     const MapPixelDeltaInt &size) const
{
    // Zero-initialized memory block (notice the parentheses)
    MapRegion result(std::shared_ptr<unsigned int>(
                     new unsigned int[size.x*size.y](),
                     ArrayDeleter<unsigned int>()),
             size.x, size.y);

    unsigned int *dest = result.GetRawData();
    for (int x = 0; x < size.x; x++) {
        if ((pos.x + x) % 10 == 0) {
            ClippedLine(dest, size,
                        MapPixelCoordInt(x, 0),
                        MapPixelCoordInt(x, size.y), 0xFF000000);
        }
    }
    for (int y = 0; y < size.y; y++) {
        if ((pos.y + y) % 10 == 0) {
            ClippedLine(dest, size,
                        MapPixelCoordInt(0, y),
                        MapPixelCoordInt(size.x, y), 0xFF000000);
        }
    }
    return result;
}

bool Gridlines::PixelToPCS(double *x, double *y) const {
    *x = *x / m_size.x * 360;
    *y = 90 - *y / m_size.y * 180;
    return true;
}

bool Gridlines::PCSToPixel(double *x, double *y) const {
    *x = *x / 360 * m_size.x;
    *y = (90 - *y) / 180 * m_size.y;
    return true;
}

Projection Gridlines::GetProj() const {
    return Projection("");
}

bool
Gridlines::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
    double x = pos.x;
    double y = pos.y;
    if (!PixelToPCS(&x, &y))
        return false;
    *result = LatLon(y, x);
    return true;
}

bool
Gridlines::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
    double x = pos.lon;
    double y = pos.lat;
    if (!PCSToPixel(&x, &y))
        return false;
    *result = MapPixelCoord(x, y);
    return true;
}

MapRegion Gridlines::GetRegionDirect(
        const MapPixelDeltaInt &output_size, const GeoPixels &base,
        const MapPixelCoord &base_tl, const MapPixelCoord &base_br) const
{
    LatLon point;
    double lat_min, lat_max, lon_min, lon_max;
    lat_min = lon_min = std::numeric_limits<double>::max();
    lat_max = lon_max = std::numeric_limits<double>::min();

    // Iterate along the display border and find min/max LatLon
    MapPixelCoordInt base_tl_int = MapPixelCoordInt(base_tl);
    MapPixelCoordInt base_br_int = MapPixelCoordInt(base_br);
    for (BorderIterator it(base_tl_int, base_br_int); !it.HasEnded(); ++it) {
        if (!base.PixelToLatLon(MapPixelCoord(*it), &point))
            return MapRegion();
        if (point.lat < lat_min) lat_min = point.lat;
        if (point.lon < lon_min) lon_min = point.lon;
        if (point.lat > lat_max) lat_max = point.lat;
        if (point.lon > lon_max) lon_max = point.lon;
    }

    double line_spacing = GetLineSpacing(lat_max - lat_min, lon_max - lon_min);
    double lat_start = lat_min + line_spacing - fmod(lat_min, line_spacing);
    double lon_start = lon_min + line_spacing - fmod(lon_min, line_spacing);

    std::shared_ptr<unsigned int> data(
                new unsigned int[output_size.x * output_size.y](),
                ArrayDeleter<unsigned int>());
    double scaling = output_size.x / (base_br.x - base_tl.x);
    for (double lat = lat_start; lat < lat_max; lat += line_spacing) {
        LatLon ll_start(lat, lon_min);
        LatLon ll_end(lat, lon_max);
        MapPixelCoord map_start, map_end;
        if (!LatLonToScreen(ll_start, base, base_tl, scaling, &map_start) ||
            !LatLonToScreen(ll_end, base, base_tl, scaling, &map_end) ||
            !BisectLine(data.get(), output_size, map_start, map_end,
                        ll_start, ll_end, base, base_tl, scaling))
        {
            return MapRegion();
        }
    }
    for (double lon = lon_start; lon < lon_max; lon += line_spacing) {
        LatLon ll_start(lat_min, lon);
        LatLon ll_end(lat_max, lon);
        MapPixelCoord map_start, map_end;
        if (!LatLonToScreen(ll_start, base, base_tl, scaling, &map_start) ||
            !LatLonToScreen(ll_end, base, base_tl, scaling, &map_end) ||
            !BisectLine(data.get(), output_size, map_start, map_end,
                        ll_start, ll_end, base, base_tl, scaling))
        {
            return MapRegion();
        }
    }
    return MapRegion(data, output_size.x, output_size.y);
}

bool Gridlines::LatLonToScreen(const LatLon &latlon,
                               const GeoPixels &base,
                               const MapPixelCoord &base_tl,
                               double scale_factor,
                               MapPixelCoord *result) const
{

    MapPixelDelta base_tl_as_delta(base_tl.x, base_tl.y);
    MapPixelCoord map_point;
    if (!base.LatLonToPixel(latlon, &map_point)) {
        return false;
    }
    *result = MapPixelCoord((map_point.x - base_tl_as_delta.x) * scale_factor,
                            (map_point.y - base_tl_as_delta.y) * scale_factor);
    return true;
}

bool Gridlines::BisectLine(unsigned int *dest,
                           const MapPixelDeltaInt &output_size,
                           const MapPixelCoord &map_start,
                           const MapPixelCoord &map_end,
                           const LatLon &ll_start,
                           const LatLon &ll_end,
                           const GeoPixels &base,
                           const MapPixelCoord &base_tl,
                           double scale_factor) const
{
    LatLon ll_mid = LatLon((ll_start.lat + ll_end.lat) / 2,
                           (ll_start.lon + ll_end.lon) / 2);
    MapPixelCoord map_mid_ll;
    if (!LatLonToScreen(ll_mid, base, base_tl, scale_factor, &map_mid_ll)) {
        return false;
    }
    MapPixelCoord map_mid_map((map_start.x + map_end.x) / 2,
                              (map_start.y + map_end.y) / 2);

    MapPixelDelta midpoint_distance = map_mid_map - map_mid_ll;
    double midpoint_distance_abs = sqrt(
            midpoint_distance.x * midpoint_distance.x +
            midpoint_distance.y * midpoint_distance.y);
    if (midpoint_distance_abs < 2.0) {
        ClippedLine(dest, output_size, MapPixelCoordInt(map_start),
                    MapPixelCoordInt(map_mid_ll), 0xFF000000);
        ClippedLine(dest, output_size, MapPixelCoordInt(map_mid_ll),
                    MapPixelCoordInt(map_end), 0xFF000000);
        return true;
    } else {
        return BisectLine(dest, output_size, map_start, map_mid_ll,
                          ll_start, ll_mid, base, base_tl, scale_factor) &&
               BisectLine(dest, output_size, map_mid_ll, map_end,
                          ll_mid, ll_end, base, base_tl, scale_factor);
    }
}

double Gridlines::GetLineSpacing(double lat_degrees, double lon_degrees) const {
    double avg_degrees = (lat_degrees + lon_degrees) / 2.0;
    if (avg_degrees < 1.0) {
        return 0.1;
    } else if (avg_degrees < 5.0) {
        return 0.5;
    } else {
        return 1.0;
    }
}

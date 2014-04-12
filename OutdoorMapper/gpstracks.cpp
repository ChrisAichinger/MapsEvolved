#include "gpstracks.h"

#include <limits>
#include <algorithm>
#include <tuple>
#include <cassert>
#define _USE_MATH_DEFINES
#include <math.h>

#include "drawing.h"


// Estimate m/degree to set a sane rendering resolution for GetRegion().
static const int EARTH_RADIUS_METERS = 6371000;
static const double METERS_PER_DEGREE = EARTH_RADIUS_METERS * M_PI / 180;
static const int METERS_PER_PIXEL = 10;


GPSSegment::GPSSegment(const std::wstring &fname,
                       const std::vector<LatLon> &points)
    : m_fname(fname), m_points(points),
      m_lat_min(std::numeric_limits<double>::max()),
      m_lon_min(std::numeric_limits<double>::max()),
      m_lat_max(std::numeric_limits<double>::min()),
      m_lon_max(std::numeric_limits<double>::min()),
      m_size()
{
    for (auto it = points.cbegin(); it != points.cend(); ++it) {
        m_lat_min = std::min(m_lat_min, it->lat);
        m_lon_min = std::min(m_lon_min, it->lon);
        m_lat_max = std::max(m_lat_max, it->lat);
        m_lon_max = std::max(m_lon_max, it->lon);
    }
    double d_lat = (m_lat_max - m_lat_min);
    double d_lon = (m_lon_max - m_lon_min);
    m_lat_min -= 0.05 * d_lat;
    m_lon_min -= 0.05 * d_lon;
    m_lat_max += 0.05 * d_lat;
    m_lon_max += 0.05 * d_lon;
    double lat_delta_meters = (m_lat_max - m_lat_min) * METERS_PER_DEGREE;
    double lat_delta_pixels = lat_delta_meters / METERS_PER_PIXEL;
    double lon_delta_pixels = lat_delta_pixels * (m_lon_max - m_lon_min)
                                               / (m_lat_max - m_lat_min);
    m_size.x = round_to_int(lon_delta_pixels);
    m_size.y = round_to_int(lat_delta_pixels);
}

GPSSegment::~GPSSegment() {}

MapRegion
GPSSegment::GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const
{
    // Zero-initialized memory block (notice the parentheses)
    MapRegion result(std::shared_ptr<unsigned int>(
                     new unsigned int[size.x*size.y](),
                     ArrayDeleter<unsigned int>()),
             size.x, size.y);

    MapPixelCoordInt endpos = pos + size;
    if (endpos.x < 0 || endpos.y < 0 ||
        pos.x > static_cast<int>(m_size.x) ||
        pos.y > static_cast<int>(m_size.y))
    {
        return result;
    }
    unsigned int *dest = result.GetRawData();
    for (auto it = m_points.cbegin(); it != m_points.cend(); ++it) {
        MapPixelCoord point_abs;
        if (!LatLonToPixel(*it, &point_abs)) {
            assert(false);
        }
        // point_rel is location of point within output region
        MapPixelCoord point_rel = point_abs - MapPixelDelta(pos.x, pos.y);
        if (!IsInRect(point_rel.x, point_rel.y, size.x, size.y)) {
            continue;
        }
        int x = round_to_int(point_rel.x);
        int y = round_to_int(point_rel.y);
        for (int j = y - 1; j < y + 1; j++)
            for (int i = x - 1; i < x+1; i++)
                ClippedSetPixel(dest, size, i, j, 0xFF0000FF);
    }
    return result;
}

bool GPSSegment::PixelToPCS(double *x, double *y) const {
    *x = *x / m_size.x * (m_lon_max - m_lon_min) + m_lon_min;
    *y = *y / m_size.y * (m_lat_max - m_lat_min) + m_lat_min;
    return true;
}

bool GPSSegment::PCSToPixel(double *x, double *y) const {
    *x = (*x - m_lon_min) / (m_lon_max - m_lon_min) * m_size.x;
    *y = (*y - m_lat_min) / (m_lat_max - m_lat_min) * m_size.y;
    return true;
}

Projection GPSSegment::GetProj() const {
    return Projection("");
}

bool
GPSSegment::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
    double x = pos.x;
    double y = pos.y;
    if (!PixelToPCS(&x, &y))
        return false;
    *result = LatLon(y, x);
    return true;
}

bool
GPSSegment::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
    double x = pos.lon;
    double y = pos.lat;
    if (!PCSToPixel(&x, &y))
        return false;
    *result = MapPixelCoord(x, y);
    return true;
}

MapRegion GPSSegment::GetRegionDirect(
        const MapPixelDeltaInt &output_size, const GeoPixels &base,
        const MapPixelCoord &base_tl, const MapPixelCoord &base_br) const
{
    MapPixelCoord base_bl(base_tl.x, base_br.y);
    MapPixelCoord base_tr(base_br.x, base_tl.y);
    LatLon latlon_tl, latlon_bl, latlon_tr, latlon_br;
    if (!base.PixelToLatLon(base_tl, &latlon_tl) ||
        !base.PixelToLatLon(base_bl, &latlon_bl) ||
        !base.PixelToLatLon(base_tr, &latlon_tr) ||
        !base.PixelToLatLon(base_br, &latlon_br))
    {
        return MapRegion();
    }

    // We might have the basemap upside-down or something like that, so don't
    // rely on latlon_topleft being minimal.

    double lat_values[] = {latlon_tl.lat, latlon_bl.lat,
                           latlon_tr.lat, latlon_br.lat};
    double lon_values[] = {latlon_tl.lon, latlon_bl.lon,
                           latlon_tr.lon, latlon_br.lon};
    double *lat_min, *lat_max, *lon_min, *lon_max;
    using std::tie; using std::minmax_element;
    tie(lat_min, lat_max) = minmax_element(&lat_values[0], &lat_values[4]);
    tie(lon_min, lon_max) = minmax_element(&lon_values[0], &lat_values[4]);

    if (m_lat_min > *lat_max || m_lat_max < *lat_min ||
        m_lon_min > *lon_max || m_lon_max < *lon_min)
    {
        // Return zero-initialized memory block (notice the parentheses)
        return MapRegion(std::shared_ptr<unsigned int>(
                    new unsigned int[output_size.x * output_size.y](),
                    ArrayDeleter<unsigned int>()),
                output_size.x, output_size.y);
    }
    std::shared_ptr<unsigned int> data(
                new unsigned int[output_size.x * output_size.y](),
                ArrayDeleter<unsigned int>());
    unsigned int *dest = data.get();
    double base_scale_factor = output_size.x / (base_br.x - base_tl.x);
    MapPixelCoordInt old_point(0, 0);
    for (auto it = m_points.cbegin(); it != m_points.cend(); ++it) {
        MapPixelCoord point_abs;
        if (!base.LatLonToPixel(*it, &point_abs)) {
            assert(false);
        }
        MapPixelCoordInt point_disp(  // Really a DisplayCoordInt
                round_to_int((point_abs.x - base_tl.x) * base_scale_factor),
                round_to_int((point_abs.y - base_tl.y) * base_scale_factor));
        int x = point_disp.x;
        int y = point_disp.y;
        for (int j = y - 2; j < y + 2; j++)
            for (int i = x - 2; i < x+2; i++)
                ClippedSetPixel(dest, output_size, i, j, 0xFF0000FF);

        if (it != m_points.cbegin()) {
            // Not the first point -> old_point is valid
            ClippedLine(dest, output_size, old_point, point_disp, 0xFF0000FF);
        }
        old_point = point_disp;

    }
    return MapRegion(data, output_size.x, output_size.y);
}

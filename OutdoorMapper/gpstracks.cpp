#include "gpstracks.h"

#include <limits>
#include <algorithm>
#include <cassert>
#define _USE_MATH_DEFINES
#include <math.h>

// Roughly estimate m/degree to set a sane rendering resolution.
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
#define CLIPPED_SET_DEST(xx,yy, val)                                      \
        if ((xx) >= 0 && (yy) >= 0 && (xx) < size.x && (yy) < size.y) {   \
            dest[(xx) + size.x * (yy)] = (val);                           \
        }

        //CLIPPED_SET_DEST(x, size.y-1-y, 0x000000FF);
        /*CLIPPED_SET_DEST(x+1, y, 0x00FF0000);
        CLIPPED_SET_DEST(x-1, y, 0x00FF0000);
        CLIPPED_SET_DEST(x, y+1, 0x00FF0000);
        CLIPPED_SET_DEST(x, y-1, 0x00FF0000);
        for (int j = 0; j < size.y; j++)
            for (int i = 0; i < size.x; i++)
                //CLIPPED_SET_DEST(i, j, 0x0000FF00);
                if ((i) >= 0 && (j) >= 0 && (i) < size.x && (j) < size.y) {
                    //dest[(i) + size.x * (j)] = 0x00FFFF00;
                }*/
        for (int j = y - 1; j < y + 1; j++)
            for (int i = x - 1; i < x+1; i++)
                //CLIPPED_SET_DEST(i, j, 0x0000FF00);
                if ((i) >= 0 && (j) >= 0 && (i) < size.x && (j) < size.y) {
                    dest[(i) + size.x * (size.y-j-1)] = 0x000000FF;
                }
#undef CLIPPED_SET_DEST
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

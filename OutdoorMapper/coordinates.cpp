
#include "coordinates.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <GeographicLib/UTMUPS.hpp>

#include "util.h"

#define OPERATORS_COORD_ADDSUB(Coord, Delta, x, y)                            \
    Coord &Coord::operator+=(const Delta &rhs) {                              \
        x += rhs.x;                                                           \
        y += rhs.y;                                                           \
        return *this;                                                         \
    }                                                                         \
    Coord &Coord::operator-=(const Delta &rhs) {                              \
        x -= rhs.x;                                                           \
        y -= rhs.y;                                                           \
        return *this;                                                         \
    }

#define OPERATORS_COORD_MULDIV(Coord, x, y)                                   \
    Coord &Coord::operator*=(double factor) {                                 \
        x *= factor;                                                          \
        y *= factor;                                                          \
        return *this;                                                         \
    }                                                                         \
    Coord &Coord::operator/=(double divisor) {                                \
        x /= divisor;                                                         \
        y /= divisor;                                                         \
        return *this;                                                         \
    }

#define OPERATORS_DELTA_ADDSUB(Class, x, y)                                   \
     Class &Class::operator+=(const Class &rhs) {                             \
         x += rhs.x;                                                          \
         y += rhs.y;                                                          \
         return *this;                                                        \
     }                                                                        \
     Class &Class::operator-=(const Class &rhs) {                             \
         x -= rhs.x;                                                          \
         y -= rhs.y;                                                          \
         return *this;                                                        \
     }

#define OPERATORS_DELTA_MULDIV(Class, MulType, x, y)                          \
     Class& Class::operator*=(MulType factor) {                               \
         x *= factor;                                                         \
         y *= factor;                                                         \
         return *this;                                                        \
     }                                                                        \
     Class& Class::operator/=(MulType divisor) {                              \
         x /= divisor;                                                        \
         y /= divisor;                                                        \
         return *this;                                                        \
     }


// DisplayCoord
OPERATORS_COORD_ADDSUB(DisplayCoord, DisplayDelta, x, y);


// DisplayCoordCentered
OPERATORS_COORD_ADDSUB(DisplayCoordCentered, DisplayDelta, x, y)
OPERATORS_COORD_MULDIV(DisplayCoordCentered, x, y)


// DisplayDelta
OPERATORS_DELTA_ADDSUB(DisplayDelta, x, y)
OPERATORS_DELTA_MULDIV(DisplayDelta, double, x, y)


// MapPixelCoord
MapPixelCoord::MapPixelCoord(const class MapPixelCoordInt &src)
    : x(src.x), y(src.y)
{};

MapPixelCoord::MapPixelCoord(const class MapPixelDelta &src)
    : x(src.x), y(src.y)
{};

void MapPixelCoord::ClampToRect(const MapPixelCoord &min_point,
                                const MapPixelCoord &max_point)
{
    if (x < min_point.x) x = min_point.x;
    if (y < min_point.y) y = min_point.y;
    if (x > max_point.x) x = max_point.x;
    if (y > max_point.y) y = max_point.y;
}

void MapPixelCoord::ClampToRect(const MapPixelCoordInt &min_point,
                                const MapPixelCoordInt &max_point)
{
    if (x < min_point.x) x = min_point.x;
    if (y < min_point.y) y = min_point.y;
    if (x > max_point.x) x = max_point.x;
    if (y > max_point.y) y = max_point.y;
}

bool MapPixelCoord::IsInRect(const MapPixelCoord &topright,
                             const MapPixelDelta &dimension)
{
    return (x >= topright.x) && (y >= topright.y) &&
           (x < topright.x + dimension.x) &&
           (y < topright.y + dimension.y);
}

bool MapPixelCoord::IsInRect(const MapPixelCoordInt &topright,
                             const MapPixelDeltaInt &dimension)
{
    return (x >= topright.x) && (y >= topright.y) &&
           (x < topright.x + dimension.x) &&
           (y < topright.y + dimension.y);
}


OPERATORS_COORD_ADDSUB(MapPixelCoord, MapPixelDelta, x, y);

// MapPixelDelta
MapPixelDelta::MapPixelDelta(const MapPixelDeltaInt &src)
    : x(src.x), y(src.y) {}

OPERATORS_DELTA_ADDSUB(MapPixelDelta, x, y)
OPERATORS_DELTA_MULDIV(MapPixelDelta, double, x, y)

// BaseMapCoord
BaseMapCoord::BaseMapCoord(const class BaseMapDelta &src)
    : MapPixelCoord(src.x, src.y) {};

OPERATORS_COORD_ADDSUB(BaseMapCoord, BaseMapDelta, x, y);

// BaseMapDelta
BaseMapDelta::BaseMapDelta(const class BaseMapCoord &src)
    : MapPixelDelta(src.x, src.y) {};

OPERATORS_DELTA_ADDSUB(BaseMapDelta, x, y)
OPERATORS_DELTA_MULDIV(BaseMapDelta, double, x, y)

// MapPixelCoordInt
MapPixelCoordInt::MapPixelCoordInt(const class MapPixelCoord &coord)
    : x(round_to_int(coord.x)), y(round_to_int(coord.y))
{}

MapPixelCoordInt::MapPixelCoordInt(const class MapPixelCoordInt &coord,
                                   int tile_size)
    : x(round_to_neg_inf(coord.x, tile_size)),
      y(round_to_neg_inf(coord.y, tile_size))
{}

MapPixelCoordInt::MapPixelCoordInt(const class MapPixelCoord &coord,
                                   int tile_size)
    : x(round_to_neg_inf(coord.x, tile_size)),
      y(round_to_neg_inf(coord.y, tile_size))
{}

MapPixelCoordInt::MapPixelCoordInt(const class MapPixelDeltaInt &src)
    : x(src.x), y(src.y)
{}

void MapPixelCoordInt::ClampToRect(const MapPixelCoordInt &min_point,
                                   const MapPixelCoordInt &max_point)
{
    if (x < min_point.x) x = min_point.x;
    if (y < min_point.y) y = min_point.y;
    if (x > max_point.x) x = max_point.x;
    if (y > max_point.y) y = max_point.y;
}

OPERATORS_COORD_ADDSUB(MapPixelCoordInt, MapPixelDeltaInt, x, y);

// MapPixelDeltaInt
MapPixelDeltaInt::MapPixelDeltaInt(const class MapPixelDelta &coord)
    : x(round_to_int(coord.x)), y(round_to_int(coord.y))
{}

OPERATORS_DELTA_ADDSUB(MapPixelDeltaInt, x, y)
OPERATORS_DELTA_MULDIV(MapPixelDeltaInt, int, x, y)

// LatLon
OPERATORS_COORD_ADDSUB(LatLon, LatLonDelta, lat, lon)

LatLon::LatLon(const UTMUPS &utm) {
    GeographicLib::UTMUPS::Reverse(utm.zone, utm.northp, utm.x, utm.y,
                                   lat, lon);
}

// LatLonDelta
OPERATORS_DELTA_ADDSUB(LatLonDelta, lat, lon)


UTMUPS::UTMUPS(const LatLon &ll) {
    GeographicLib::UTMUPS::Forward(ll.lat, ll.lon, zone, northp, x, y);
};


// MapPixelGradient
double MapBezierGradient::Abs() const {
    return sqrt(x*x + y*y);
}
OPERATORS_COORD_MULDIV(MapBezierGradient, x, y)


BorderIterator::BorderIterator(const MapPixelCoordInt &rect_tl,
                              const MapPixelCoordInt &rect_br)
    : m_tl(rect_tl), m_br(rect_br), m_value(rect_tl), m_pos(0)
{}

BorderIterator& BorderIterator::operator=(BorderIterator rhs) {
    std::swap(*this, rhs);
    return *this;
}

bool BorderIterator::operator==(const BorderIterator& rhs) const {
    if (m_tl != rhs.m_tl || m_br != rhs.m_br)
        return false;
    if (HasEnded() && rhs.HasEnded())
        return true;
    return m_value == rhs.m_value && m_pos == rhs.m_pos;
}

bool BorderIterator::operator!=(const BorderIterator& rhs) const {
    return !(*this == rhs);
}

BorderIterator& BorderIterator::operator++() {
    Advance(+1);
    return *this;
}
BorderIterator& BorderIterator::operator--() {
    Advance(-1);
    return *this;
}
BorderIterator BorderIterator::operator++(int) {
    BorderIterator tmp(*this);
    operator++();
    return tmp;
}
BorderIterator BorderIterator::operator--(int) {
    BorderIterator tmp(*this);
    operator--();
    return tmp;
}

BorderIterator::const_reference BorderIterator::operator*() const {
    return m_value;
}

BorderIterator::const_pointer BorderIterator::operator->() const {
    return &m_value;
}

bool BorderIterator::HasEnded() const {
    return abs(m_pos) >= 2 * (m_br.x - m_tl.x) + 2 * (m_br.y - m_tl.y);
}

BorderIterator BorderIterator::End() const {
    BorderIterator result(*this);
    result.m_pos = std::numeric_limits<int>::max();
    return result;
}

void BorderIterator::Advance(int increment) {
    assert(increment == 1 || increment == -1);
    if (m_value.y == m_tl.y && m_value.x != m_br.x) {
        m_value.x += increment;  // top border
    }
    if (m_value.x == m_br.x && m_value.y != m_br.y) {
        m_value.y += increment;  // right border
    }
    if (m_value.y == m_br.y && m_value.x != m_tl.x) {
        m_value.x -= increment;  // bottom border
    }
    if (m_value.x == m_tl.x && m_value.y != m_tl.y) {
        m_value.y -= increment;  // left border
    }
    m_pos += increment;
}

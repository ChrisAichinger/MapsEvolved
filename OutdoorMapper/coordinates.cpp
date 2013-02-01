#include <cmath>

#include "coordinates.h"
#include "disp_ogl.h"
#include "mapdisplay.h"
#include "rastermap.h"

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

#define OPERATORS_DELTA_MULDIV(Class, x, y)                                   \
     Class& Class::operator*=(double factor) {                                \
         x *= factor;                                                         \
         y *= factor;                                                         \
         return *this;                                                        \
     }                                                                        \
     Class& Class::operator/=(double divisor) {                               \
         x /= divisor;                                                        \
         y /= divisor;                                                        \
         return *this;                                                        \
     }

// DisplayCoord
OPERATORS_COORD_ADDSUB(DisplayCoord, DisplayDelta, x, y);

// DisplayCoordCentered
DisplayCoordCentered::DisplayCoordCentered(const DisplayCoord &dc,
                                           const Display &disp)
    : x(dc.x - disp.GetDisplayWidth() / 2.0),
      y(dc.y - disp.GetDisplayHeight() / 2.0)
{}

DisplayCoordCentered::DisplayCoordCentered(const MapPixelCoord &mpc,
                                           const MapDisplayManager &mapdisplay)
    : x((mpc.x - mapdisplay.GetCenterX()) * mapdisplay.GetZoom()),
      y((mpc.y - mapdisplay.GetCenterY()) * mapdisplay.GetZoom())
{}

DisplayCoordCentered::DisplayCoordCentered(const MapPixelCoordInt &mpc,
                                           const MapDisplayManager &mapdisplay)
    : x((mpc.x - mapdisplay.GetCenterX()) * mapdisplay.GetZoom()),
      y((mpc.y - mapdisplay.GetCenterY()) * mapdisplay.GetZoom())
{}


OPERATORS_COORD_ADDSUB(DisplayCoordCentered, DisplayDelta, x, y)
OPERATORS_COORD_MULDIV(DisplayCoordCentered, x, y)

// DisplayDelta
OPERATORS_DELTA_ADDSUB(DisplayDelta, x, y)
OPERATORS_DELTA_MULDIV(DisplayDelta, x, y)


// MapPixelCoord
MapPixelCoord::MapPixelCoord(const class DisplayCoordCentered &dc,
                             const class MapDisplayManager &mapdisplay)
    : x(mapdisplay.GetCenterX() + dc.x / mapdisplay.GetZoom()),
      y(mapdisplay.GetCenterY() + dc.y / mapdisplay.GetZoom())
{}

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

bool MapPixelCoord::IsInRect(const MapPixelCoord &topright,
                             const MapPixelDelta &dimension)
{
    return (x >= topright.x) && (y >= topright.y) &&
           (x <= topright.x + dimension.x) &&
           (y <= topright.y + dimension.y);
}

OPERATORS_COORD_ADDSUB(MapPixelCoord, MapPixelDelta, x, y);

// MapPixelDelta
MapPixelDelta::MapPixelDelta(const class DisplayDelta &dd,
                             const class MapDisplayManager &mapdisplay)
    : x(dd.x / mapdisplay.GetZoom()), y(dd.y / mapdisplay.GetZoom())
{}

OPERATORS_DELTA_ADDSUB(MapPixelDelta, x, y)
OPERATORS_DELTA_MULDIV(MapPixelDelta, x, y)

// MapPixelCoordInt
MapPixelCoordInt::MapPixelCoordInt(const class MapPixelCoord &coord)
    : x(round_to_int(coord.x)), y(round_to_int(coord.y))
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

// LatLon
LatLon::LatLon(const MapPixelCoord &pos, const class RasterMap &map) {
    double x = pos.x;
    double y = pos.y;
    LatLon ll = map.PixelToLatLon(pos);
    lat = ll.lat;
    lon = ll.lon;
}
OPERATORS_COORD_ADDSUB(LatLon, LatLonDelta, lat, lon)

// LatLonDelta
OPERATORS_DELTA_ADDSUB(LatLonDelta, lat, lon)

// MapPixelGradient
double MapBezierGradient::Abs() const {
    return sqrt(x*x + y*y);
}
OPERATORS_COORD_MULDIV(MapBezierGradient, x, y)

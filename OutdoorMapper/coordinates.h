#ifndef ODM__COORDINATES_H
#define ODM__COORDINATES_H

#include <ostream>

class BezierCoord {
    public:
        BezierCoord() : x(0), y(0) {};
        BezierCoord(double x_, double y_) : x(x_), y(y_) {};
        double x, y;
};

class DisplayCoord {
    public:
        DisplayCoord() : x(0), y(0) {};
        DisplayCoord(double x_, double y_) : x(x_), y(y_) {};
        double x, y;

        DisplayCoord& operator+=(const class DisplayDelta &rhs);
        DisplayCoord& operator-=(const class DisplayDelta &rhs);
};

class DisplayCoordCentered {
    public:
        DisplayCoordCentered() : x(0), y(0) {};
        DisplayCoordCentered(double x_, double y_) : x(x_), y(y_) {};
        DisplayCoordCentered(const DisplayCoord &dc,
                             const class Display &disp);
        DisplayCoordCentered(const class MapPixelCoord &mpc,
                             const class MapDisplayManager &mapdisplay);
        DisplayCoordCentered(const class MapPixelCoordInt &mpc,
                             const class MapDisplayManager &mapdisplay);

        DisplayCoordCentered& operator+=(const class DisplayDelta &rhs);
        DisplayCoordCentered& operator-=(const class DisplayDelta &rhs);
        DisplayCoordCentered& operator*=(double factor);
        DisplayCoordCentered& operator/=(double divisor);

        double x, y;
};

class DisplayDelta {
    public:
        DisplayDelta() : x(0), y(0) {};
        DisplayDelta(double x_, double y_) : x(x_), y(y_) {};

        DisplayDelta& operator+=(const class DisplayDelta &rhs);
        DisplayDelta& operator-=(const class DisplayDelta &rhs);
        DisplayDelta& operator*=(double factor);
        DisplayDelta& operator/=(double divisor);

        double x, y;
};


class MapPixelCoord {
    public:
        MapPixelCoord() : x(0), y(0) {};
        MapPixelCoord(double x_, double y_) : x(x_), y(y_) {};
        MapPixelCoord(const class DisplayCoordCentered &dc,
                      const class MapDisplayManager &mapdisplay);
        explicit MapPixelCoord(const class MapPixelDelta &src);
        explicit MapPixelCoord(const class MapPixelCoordInt &src);

        MapPixelCoord& operator+=(const class MapPixelDelta &rhs);
        MapPixelCoord& operator-=(const class MapPixelDelta &rhs);

        void ClampToRect(const MapPixelCoord &min_point,
                         const MapPixelCoord &max_point);
        bool IsInRect(const MapPixelCoord &topright,
                      const MapPixelDelta &dimension);

        double x, y;
};

class MapPixelDelta {
    public:
        MapPixelDelta() : x(0), y(0) {};
        MapPixelDelta(double x_, double y_) : x(x_), y(y_) {};
        MapPixelDelta(const class DisplayDelta &dd,
                      const class MapDisplayManager &mapdisplay);

        MapPixelDelta& operator+=(const class MapPixelDelta &rhs);
        MapPixelDelta& operator-=(const class MapPixelDelta &rhs);
        MapPixelDelta& operator*=(double factor);
        MapPixelDelta& operator/=(double divisor);


        double x, y;
};

class MapPixelCoordInt {
    public:
        MapPixelCoordInt() : x(0), y(0) {};
        MapPixelCoordInt(int x_, int y_) : x(x_), y(y_) {};
        explicit MapPixelCoordInt(const class MapPixelDeltaInt &src);

        // Round MapPixelCoord to nearest pixel
        explicit MapPixelCoordInt(const class MapPixelCoord &coord);

        // Find top-left tile corner from MapPixelCoord
        MapPixelCoordInt(const class MapPixelCoord &coord, int tile_size);

        MapPixelCoordInt& operator+=(const class MapPixelDeltaInt &rhs);
        MapPixelCoordInt& operator-=(const class MapPixelDeltaInt &rhs);

        void ClampToRect(const MapPixelCoordInt &min_point,
                         const MapPixelCoordInt &max_point);

        int x, y;
};

class MapPixelDeltaInt {
    public:
        MapPixelDeltaInt() : x(0), y(0) {};
        MapPixelDeltaInt(int x_, int y_) : x(x_), y(y_) {};
        MapPixelDeltaInt(const class MapPixelDelta &coord);

        MapPixelDeltaInt& operator+=(const class MapPixelDeltaInt &rhs);
        MapPixelDeltaInt& operator-=(const class MapPixelDeltaInt &rhs);

        int x, y;
};

class LatLon {
    public:
        LatLon() : lat(0), lon(0) {};
        LatLon(double lat_, double lon_) : lat(lat_), lon(lon_) {};
        LatLon(const MapPixelCoord &pos, const class RasterMap &map);

        LatLon& operator+=(const class LatLonDelta &rhs);
        LatLon& operator-=(const class LatLonDelta &rhs);

        double lat, lon;
};

class LatLonDelta {
    public:
        LatLonDelta() : lat(0), lon(0) {};
        LatLonDelta(double lat_, double lon_) : lat(lat_), lon(lon_) {};

        LatLonDelta& operator+=(const class LatLonDelta &rhs);
        LatLonDelta& operator-=(const class LatLonDelta &rhs);

        double lat, lon;
};

class MapBezierGradient {
    public:
        MapBezierGradient() : x(0), y(0) {};
        MapBezierGradient(double x_, double y_) : x(x_), y(y_) {};

        MapBezierGradient& operator*=(double factor);
        MapBezierGradient& operator/=(double divisor);

        double Abs() const;

        double x, y;
};



#define OPERATORS_EQ_STREAM(Class)                                            \
    inline bool operator==(const Class& lhs, const Class& rhs) {              \
        return (lhs.x == rhs.x) && (lhs.y == rhs.y);                          \
    }                                                                         \
    inline bool operator!=(const Class& lhs, const Class& rhs) {              \
        return !operator==(lhs, rhs);                                         \
    }                                                                         \
    inline std::ostream& operator<<(std::ostream& os, const Class& obj) {     \
        os << #Class << "(" << obj.x << ", " << obj.y << ")";                 \
        return os;                                                            \
    }                                                                         \
    inline std::wostream& operator<<(std::wostream& os, const Class& obj) {   \
        os << #Class << "(" << obj.x << ", " << obj.y << ")";                 \
        return os;                                                            \
    }

#define OPERATORS_MULDIV(Class, MulType)                                      \
    inline Class operator*(Class lhs, MulType rhs) {                          \
        lhs *= rhs;                                                           \
        return lhs;                                                           \
    }                                                                         \
    inline Class operator*(MulType lhs, Class rhs) {                          \
        rhs *= lhs;                                                           \
        return rhs;                                                           \
    }                                                                         \
    inline Class operator/(Class lhs, MulType rhs) {                          \
        lhs /= rhs;                                                           \
        return lhs;                                                           \
    }

#define OPERATORS_ADDSUB(Class, Delta)                                        \
    inline Class operator+(Class lhs, const Delta &rhs) {                     \
        lhs += rhs;                                                           \
        return lhs;                                                           \
    }                                                                         \
    inline Class operator+(const Delta &lhs, Class rhs) {                     \
        rhs += lhs;                                                           \
        return rhs;                                                           \
    }                                                                         \
    inline Class operator-(Class lhs, const Delta &rhs) {                     \
        lhs -= rhs;                                                           \
        return lhs;                                                           \
    }

#define OPERATORS_ADDSUB1(Class)                                              \
    inline Class operator+(Class lhs, const Class &rhs) {                     \
        lhs += rhs;                                                           \
        return lhs;                                                           \
    }                                                                         \
    inline Class operator-(Class lhs, const Class &rhs) {                     \
        lhs -= rhs;                                                           \
        return lhs;                                                           \
    }

OPERATORS_EQ_STREAM(DisplayCoord)
OPERATORS_EQ_STREAM(DisplayCoordCentered)
OPERATORS_EQ_STREAM(DisplayDelta)
OPERATORS_EQ_STREAM(MapPixelCoord)
OPERATORS_EQ_STREAM(MapPixelDelta)
OPERATORS_EQ_STREAM(MapPixelCoordInt)
OPERATORS_EQ_STREAM(MapPixelDeltaInt)
OPERATORS_EQ_STREAM(MapBezierGradient)

OPERATORS_MULDIV(DisplayDelta, double)
OPERATORS_MULDIV(DisplayCoordCentered, double)
OPERATORS_MULDIV(MapPixelDelta, double)
OPERATORS_MULDIV(MapBezierGradient, double)

OPERATORS_ADDSUB(DisplayCoord, DisplayDelta)
OPERATORS_ADDSUB(DisplayCoordCentered, DisplayDelta)
OPERATORS_ADDSUB(MapPixelCoord, MapPixelDelta)
OPERATORS_ADDSUB(MapPixelCoordInt, MapPixelDeltaInt)

OPERATORS_ADDSUB1(DisplayDelta)
OPERATORS_ADDSUB1(MapPixelDelta)
OPERATORS_ADDSUB1(MapPixelDeltaInt)
OPERATORS_ADDSUB1(LatLonDelta)

inline DisplayDelta
operator-(const DisplayCoord &lhs, const DisplayCoord &rhs) {
    return DisplayDelta(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline DisplayDelta
operator-(const DisplayCoordCentered &lhs, const DisplayCoordCentered &rhs) {
    return DisplayDelta(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline MapPixelDelta
operator-(const MapPixelCoord &lhs, const MapPixelCoord &rhs) {
    return MapPixelDelta(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline MapPixelDeltaInt
operator-(const MapPixelCoordInt &lhs, const MapPixelCoordInt &rhs) {
    return MapPixelDeltaInt(lhs.x - rhs.x, lhs.y - rhs.y);
}



#endif

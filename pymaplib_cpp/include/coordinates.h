#ifndef ODM__COORDINATES_H
#define ODM__COORDINATES_H

#include <ostream>
#include <algorithm>
#include "odm_config.h"

// UnitSquareCoord represents coordinates in the unit square (x, y in 0..1).
//
// Usecases include Bezier calculations, where a 2D Bezier surface spans the
// unit square.
//
// Values outside (0, 1) are valid and can have well-defined meaning,
// depending on the context. For example, map coordinates can be normalized to
// the unit square. Then points inside (0, 1) are on the map, while points
// outside (0, 1) are not.
class EXPORT UnitSquareCoord {
    public:
        UnitSquareCoord() : x(0), y(0) {};
        UnitSquareCoord(double x_, double y_) : x(x_), y(y_) {};
        double x, y;
};

class EXPORT PixelBufCoord {
    public:
        PixelBufCoord() : x(0), y(0) {};
        PixelBufCoord(int x_, int y_) : x(x_), y(y_) {};

        PixelBufCoord& operator+=(const class PixelBufDelta &rhs);
        PixelBufCoord& operator-=(const class PixelBufDelta &rhs);

        int x, y;
};

class EXPORT PixelBufDelta {
    public:
        PixelBufDelta() : x(0), y(0) {};
        PixelBufDelta(int x_, int y_) : x(x_), y(y_) {};

        PixelBufDelta& operator+=(const class PixelBufDelta &rhs);
        PixelBufDelta& operator-=(const class PixelBufDelta &rhs);

        int x, y;
};


class EXPORT DisplayCoord {
    public:
        DisplayCoord() : x(0), y(0) {};
        DisplayCoord(double x_, double y_) : x(x_), y(y_) {};
        double x, y;

        DisplayCoord& operator+=(const class DisplayDelta &rhs);
        DisplayCoord& operator-=(const class DisplayDelta &rhs);
};

class EXPORT DisplayCoordCentered {
    public:
        DisplayCoordCentered() : x(0), y(0) {};
        DisplayCoordCentered(double x_, double y_) : x(x_), y(y_) {};

        DisplayCoordCentered& operator+=(const class DisplayDelta &rhs);
        DisplayCoordCentered& operator-=(const class DisplayDelta &rhs);
        DisplayCoordCentered& operator*=(double factor);
        DisplayCoordCentered& operator/=(double divisor);

        double x, y;
};

class EXPORT DisplayDelta {
    public:
        DisplayDelta() : x(0), y(0) {};
        DisplayDelta(double x_, double y_) : x(x_), y(y_) {};

        DisplayDelta& operator+=(const class DisplayDelta &rhs);
        DisplayDelta& operator-=(const class DisplayDelta &rhs);
        DisplayDelta& operator*=(double factor);
        DisplayDelta& operator/=(double divisor);

        double x, y;
};

class EXPORT DisplayRectCentered {
    public:
        DisplayRectCentered(const DisplayCoordCentered &pos,
                            const DisplayDelta &size)
            : tl(pos.x,          pos.y),
              tr(pos.x + size.x, pos.y),
              bl(pos.x,          pos.y + size.y),
              br(pos.x + size.x, pos.y + size.y)
            {};
        DisplayRectCentered(const DisplayCoordCentered &TopLeft,
                         const DisplayCoordCentered &TopRight,
                         const DisplayCoordCentered &BotLeft,
                         const DisplayCoordCentered &BotRight)
            : tl(TopLeft), tr(TopRight),
              bl(BotLeft), br(BotRight)
            {};

        DisplayCoordCentered tl, tr, bl, br;
};

class EXPORT MapPixelCoord {
    public:
        MapPixelCoord() : x(0), y(0) {};
        MapPixelCoord(double x_, double y_) : x(x_), y(y_) {};
        explicit MapPixelCoord(const class MapPixelDelta &src);
        explicit MapPixelCoord(const class MapPixelCoordInt &src);

        MapPixelCoord& operator+=(const class MapPixelDelta &rhs);
        MapPixelCoord& operator-=(const class MapPixelDelta &rhs);

        void ClampToRect(const MapPixelCoord &min_point,
                         const MapPixelCoord &max_point);
        void ClampToRect(const class MapPixelCoordInt &min_point,
                         const class MapPixelCoordInt &max_point);
        bool IsInRect(const MapPixelCoord &topright,
                      const MapPixelDelta &dimension);
        bool IsInRect(const class MapPixelCoordInt &topright,
                      const class MapPixelDeltaInt &dimension);

        double x, y;
};

class EXPORT MapPixelDelta {
    public:
        MapPixelDelta() : x(0), y(0) {};
        MapPixelDelta(double x_, double y_) : x(x_), y(y_) {};
        explicit MapPixelDelta(const class MapPixelDeltaInt &src);

        MapPixelDelta& operator+=(const class MapPixelDelta &rhs);
        MapPixelDelta& operator-=(const class MapPixelDelta &rhs);
        MapPixelDelta& operator*=(double factor);
        MapPixelDelta& operator/=(double divisor);

        double x, y;
};

class EXPORT MapPixelCoordInt {
    public:
        MapPixelCoordInt() : x(0), y(0) {};
        MapPixelCoordInt(int x_, int y_) : x(x_), y(y_) {};
        explicit MapPixelCoordInt(const class MapPixelDeltaInt &src);

        // Round MapPixelCoord to nearest pixel
        explicit MapPixelCoordInt(const class MapPixelCoord &coord);

        // Find top-left tile corner from MapPixelCoord/MapPixelCoordInt
        MapPixelCoordInt(const class MapPixelCoordInt &coord, int tile_size);
        MapPixelCoordInt(const class MapPixelCoord &coord, int tile_size);

        MapPixelCoordInt& operator+=(const class MapPixelDeltaInt &rhs);
        MapPixelCoordInt& operator-=(const class MapPixelDeltaInt &rhs);

        void ClampToRect(const MapPixelCoordInt &min_point,
                         const MapPixelCoordInt &max_point);

        int x, y;
};

class EXPORT MapPixelDeltaInt {
    public:
        MapPixelDeltaInt() : x(0), y(0) {};
        MapPixelDeltaInt(int x_, int y_) : x(x_), y(y_) {};
        explicit MapPixelDeltaInt(const class MapPixelDelta &coord);

        MapPixelDeltaInt& operator+=(const class MapPixelDeltaInt &rhs);
        MapPixelDeltaInt& operator-=(const class MapPixelDeltaInt &rhs);
        MapPixelDeltaInt& operator*=(int factor);
        MapPixelDeltaInt& operator/=(int divisor);

        int x, y;
};

class EXPORT BaseMapCoord : public MapPixelCoord {
    public:
        BaseMapCoord() : MapPixelCoord() {};
        BaseMapCoord(double x_, double y_) : MapPixelCoord(x_, y_) {};
        explicit BaseMapCoord(const MapPixelCoord &mp) : MapPixelCoord(mp) {};
        explicit BaseMapCoord(const MapPixelDelta &mp) : MapPixelCoord(mp) {};
        explicit BaseMapCoord(const MapPixelCoordInt &mp) : MapPixelCoord(mp) {};
        explicit BaseMapCoord(const class BaseMapDelta &src);

        BaseMapCoord& operator+=(const class BaseMapDelta &rhs);
        BaseMapCoord& operator-=(const class BaseMapDelta &rhs);
};

class EXPORT BaseMapDelta : public MapPixelDelta {
    public:
        BaseMapDelta() : MapPixelDelta() {};
        BaseMapDelta(double x_, double y_) : MapPixelDelta(x_, y_) {};
        explicit BaseMapDelta(const MapPixelDelta &mp) : MapPixelDelta(mp) {};
        explicit BaseMapDelta(const MapPixelDeltaInt &mp) : MapPixelDelta(mp) {};
        explicit BaseMapDelta(const class BaseMapCoord &src);

        BaseMapDelta& operator+=(const class BaseMapDelta &rhs);
        BaseMapDelta& operator-=(const class BaseMapDelta &rhs);
        BaseMapDelta& operator*=(double factor);
        BaseMapDelta& operator/=(double divisor);
};

class EXPORT LatLon {
    public:
        LatLon() : lat(0), lon(0) {};
        LatLon(double lat_, double lon_) : lat(lat_), lon(lon_) {};
        explicit LatLon(const class UTMUPS &utm);

        LatLon& operator+=(const class LatLonDelta &rhs);
        LatLon& operator-=(const class LatLonDelta &rhs);

        double lat, lon;
};

class EXPORT LatLonDelta {
    public:
        LatLonDelta() : lat(0), lon(0) {};
        LatLonDelta(double lat_, double lon_) : lat(lat_), lon(lon_) {};

        LatLonDelta& operator+=(const class LatLonDelta &rhs);
        LatLonDelta& operator-=(const class LatLonDelta &rhs);

        double lat, lon;
};

class EXPORT UTMUPS {
    public:
        UTMUPS() : zone(0), northp(false), x(0), y(0) {};
        UTMUPS(int zone_, bool northp_, double x_, double y_)
            : zone(zone_), northp(northp_), x(x_), y(y_)
        {};
        explicit UTMUPS(const LatLon &ll);

        int zone;     // UTM zone; may be 0 to indicate UPS.
        bool northp;  // True if north hemisphere, false for south.
        double x, y;  // Eastings and northings.
};

class EXPORT MapBezierGradient {
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

OPERATORS_EQ_STREAM(PixelBufCoord)
OPERATORS_EQ_STREAM(PixelBufDelta)
OPERATORS_EQ_STREAM(DisplayCoord)
OPERATORS_EQ_STREAM(DisplayCoordCentered)
OPERATORS_EQ_STREAM(DisplayDelta)
OPERATORS_EQ_STREAM(MapPixelCoord)
OPERATORS_EQ_STREAM(MapPixelDelta)
OPERATORS_EQ_STREAM(MapPixelCoordInt)
OPERATORS_EQ_STREAM(MapPixelDeltaInt)
OPERATORS_EQ_STREAM(MapBezierGradient)
OPERATORS_EQ_STREAM(BaseMapCoord)
OPERATORS_EQ_STREAM(BaseMapDelta)

OPERATORS_MULDIV(DisplayDelta, double)
OPERATORS_MULDIV(DisplayCoordCentered, double)
OPERATORS_MULDIV(MapPixelDelta, double)
OPERATORS_MULDIV(BaseMapDelta, double)
OPERATORS_MULDIV(MapPixelDeltaInt, int)
OPERATORS_MULDIV(MapBezierGradient, double)

OPERATORS_ADDSUB(PixelBufCoord, PixelBufDelta)
OPERATORS_ADDSUB(DisplayCoord, DisplayDelta)
OPERATORS_ADDSUB(DisplayCoordCentered, DisplayDelta)
OPERATORS_ADDSUB(MapPixelCoord, MapPixelDelta)
OPERATORS_ADDSUB(MapPixelCoordInt, MapPixelDeltaInt)
OPERATORS_ADDSUB(BaseMapCoord, BaseMapDelta)

OPERATORS_ADDSUB1(PixelBufDelta)
OPERATORS_ADDSUB1(DisplayDelta)
OPERATORS_ADDSUB1(MapPixelDelta)
OPERATORS_ADDSUB1(MapPixelDeltaInt)
OPERATORS_ADDSUB1(LatLonDelta)
OPERATORS_ADDSUB1(BaseMapDelta)

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

inline BaseMapDelta
operator-(const BaseMapCoord &lhs, const BaseMapCoord &rhs) {
    return BaseMapDelta(lhs.x - rhs.x, lhs.y - rhs.y);
}


inline MapPixelDelta
operator*(const MapPixelDeltaInt &lhs, double rhs) {
    return MapPixelDelta(lhs.x * rhs, lhs.y * rhs);
}

inline MapPixelDelta
operator*(double lhs, const MapPixelDeltaInt &rhs) {
    return MapPixelDelta(lhs * rhs.x, lhs * rhs.y);
}

inline MapPixelDelta
operator/(const MapPixelDeltaInt &lhs, double rhs) {
    return MapPixelDelta(lhs.x / rhs, lhs.y / rhs);
}


class BorderIterator {
    /* An iterator returning points along the border of the rectangle defined
     * by rect_tl (top left corner) and rect_br (bottom right corner).
     * Iteration starts at the top-left corner, forward iteration (++)
     * proceeds clockwise, reverse iteration (--) proceeds counter-clockwise.
     *
     * The rectangle is INCLUSIVE, thus rect_br is traversed!
     */
    public:
        typedef MapPixelCoordInt value_type;
        typedef const MapPixelCoordInt &const_reference;
        typedef const MapPixelCoordInt *const_pointer;
        typedef std::bidirectional_iterator_tag iterator_category;

        BorderIterator(const MapPixelCoordInt &rect_tl,
                       const MapPixelCoordInt &rect_br);

        BorderIterator& operator=(BorderIterator);
        bool operator==(const BorderIterator&) const;
        bool operator!=(const BorderIterator&) const;

        BorderIterator& operator++();
        BorderIterator& operator--();
        BorderIterator operator++(int);
        BorderIterator operator--(int);

        const_reference operator*() const;
        const_pointer operator->() const;

        friend void swap(BorderIterator& lhs, BorderIterator& rhs) {
            // enable ADL (not necessary in our case, but good practice)
            using std::swap;

            std::swap(lhs.m_tl, rhs.m_tl);
            std::swap(lhs.m_br, rhs.m_br);
            std::swap(lhs.m_value, rhs.m_value);
            std::swap(lhs.m_pos, rhs.m_pos);
        }

        // Return true if the iterator has ended (i.e. we finished one complete
        // loop around the rectangle).
        bool HasEnded() const;
        // Use as (it == it.End()), however, HasEnded() is more efficient.
        BorderIterator End() const;
    private:
        MapPixelCoordInt m_tl;
        MapPixelCoordInt m_br;
        MapPixelCoordInt m_value;
        int m_pos;

        // Move to the next or the previous value. Increment must be +1 or -1.
        void Advance(int increment);
};

#endif

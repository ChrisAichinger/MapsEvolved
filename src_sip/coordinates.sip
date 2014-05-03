%ModuleHeaderCode
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"
%End


class BezierCoord {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        BezierCoord();
        BezierCoord(double x_, double y_);
        double x;
        double y;
};

class DisplayCoord {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        DisplayCoord();
        DisplayCoord(double x_, double y_);
        double x;
        double y;

        DisplayCoord& operator+=(const DisplayDelta &rhs);
        DisplayCoord& operator-=(const DisplayDelta &rhs);
};

class DisplayCoordCentered {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        DisplayCoordCentered();
        DisplayCoordCentered(double x_, double y_);

        DisplayCoordCentered& operator+=(const DisplayDelta &rhs);
        DisplayCoordCentered& operator-=(const DisplayDelta &rhs);
        DisplayCoordCentered& operator*=(double factor);
        DisplayCoordCentered& operator/=(double divisor);

        double x;
        double y;
};

class DisplayDelta {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        DisplayDelta();
        DisplayDelta(double x_, double y_);

        DisplayDelta& operator+=(const DisplayDelta &rhs);
        DisplayDelta& operator-=(const DisplayDelta &rhs);
        DisplayDelta& operator*=(double factor);
        DisplayDelta& operator/=(double divisor);

        double x;
        double y;
};


class MapPixelCoord {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        MapPixelCoord();
        MapPixelCoord(double x_, double y_);
        explicit MapPixelCoord(const MapPixelDelta &src);
        explicit MapPixelCoord(const MapPixelCoordInt &src);

        MapPixelCoord& operator+=(const MapPixelDelta &rhs);
        MapPixelCoord& operator-=(const MapPixelDelta &rhs);

        void ClampToRect(const MapPixelCoord &min_point,
                         const MapPixelCoord &max_point);
        void ClampToRect(const MapPixelCoordInt &min_point,
                         const MapPixelCoordInt &max_point);
        bool IsInRect(const MapPixelCoord &topright,
                      const MapPixelDelta &dimension);
        bool IsInRect(const MapPixelCoordInt &topright,
                      const MapPixelDeltaInt &dimension);

        double x;
        double y;
};

class MapPixelDelta {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        MapPixelDelta();
        MapPixelDelta(double x_, double y_);
        explicit MapPixelDelta(const MapPixelDeltaInt &src);

        MapPixelDelta& operator+=(const MapPixelDelta &rhs);
        MapPixelDelta& operator-=(const MapPixelDelta &rhs);
        MapPixelDelta& operator*=(double factor);
        MapPixelDelta& operator/=(double divisor);

        double x;
        double y;
};

class MapPixelCoordInt {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        MapPixelCoordInt();
        MapPixelCoordInt(int x_, int y_);
        explicit MapPixelCoordInt(const MapPixelDeltaInt &src);

        // Round MapPixelCoord to nearest pixel
        explicit MapPixelCoordInt(const MapPixelCoord &coord);

        // Find top-left tile corner from MapPixelCoord/MapPixelCoordInt
        MapPixelCoordInt(const MapPixelCoordInt &coord, int tile_size);
        MapPixelCoordInt(const MapPixelCoord &coord, int tile_size);

        MapPixelCoordInt& operator+=(const MapPixelDeltaInt &rhs);
        MapPixelCoordInt& operator-=(const MapPixelDeltaInt &rhs);

        void ClampToRect(const MapPixelCoordInt &min_point,
                         const MapPixelCoordInt &max_point);

        int x;
        int y;
};

class MapPixelDeltaInt {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        MapPixelDeltaInt();
        MapPixelDeltaInt(int x_, int y_);
        explicit MapPixelDeltaInt(const MapPixelDelta &coord);

        MapPixelDeltaInt& operator+=(const MapPixelDeltaInt &rhs);
        MapPixelDeltaInt& operator-=(const MapPixelDeltaInt &rhs);
        MapPixelDeltaInt& operator*=(int factor);
        MapPixelDeltaInt& operator/=(int divisor);

        int x;
        int y;
};

class BaseMapCoord : public MapPixelCoord {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        BaseMapCoord();
        BaseMapCoord(double x_, double y_);
        explicit BaseMapCoord(const MapPixelCoord &mp);
        explicit BaseMapCoord(const MapPixelDelta &mp);
        explicit BaseMapCoord(const MapPixelCoordInt &mp);
        explicit BaseMapCoord(const BaseMapDelta &src);

        BaseMapCoord& operator+=(const BaseMapDelta &rhs);
        BaseMapCoord& operator-=(const BaseMapDelta &rhs);
};

class BaseMapDelta : public MapPixelDelta {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        BaseMapDelta();
        BaseMapDelta(double x_, double y_);
        explicit BaseMapDelta(const MapPixelDelta &mp);
        explicit BaseMapDelta(const MapPixelDeltaInt &mp);
        explicit BaseMapDelta(const BaseMapCoord &src);

        BaseMapDelta& operator+=(const BaseMapDelta &rhs);
        BaseMapDelta& operator-=(const BaseMapDelta &rhs);
        BaseMapDelta& operator*=(double factor);
        BaseMapDelta& operator/=(double divisor);
};

class LatLon {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        LatLon();
        LatLon(double lat_, double lon_);
        LatLon(const UTMUPS &utm);

        LatLon& operator+=(const LatLonDelta &rhs);
        LatLon& operator-=(const LatLonDelta &rhs);

        double lat;
        double lon;
};

class LatLonDelta {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        LatLonDelta();
        LatLonDelta(double lat_, double lon_);

        LatLonDelta& operator+=(const LatLonDelta &rhs);
        LatLonDelta& operator-=(const LatLonDelta &rhs);

        double lat;
        double lon;
};

class UTMUPS {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        UTMUPS();
        UTMUPS(int zone_, bool northp_, double x_, double y_);
        UTMUPS(const LatLon &ll);

        int zone;     // UTM zone; may be 0 to indicate UPS.
        bool northp;  // True if north hemisphere, false for south.
        double x;     // UTM/UPS easting
        double y;     // UTM/UPS northing
};

class MapBezierGradient {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        MapBezierGradient();
        MapBezierGradient(double x_, double y_);

        MapBezierGradient& operator*=(double factor);
        MapBezierGradient& operator/=(double divisor);

        double Abs() const;

        double x;
        double y;
};


bool operator==(const DisplayCoord& lhs, const DisplayCoord& rhs);
bool operator!=(const DisplayCoord& lhs, const DisplayCoord& rhs);
//std::ostream& operator<<(std::ostream& os, const DisplayCoord& obj);
//std::wostream& operator<<(std::wostream& os, const DisplayCoord& obj);

bool operator==(const DisplayCoordCentered& lhs, const DisplayCoordCentered& rhs);
bool operator!=(const DisplayCoordCentered& lhs, const DisplayCoordCentered& rhs);
//std::ostream& operator<<(std::ostream& os, const DisplayCoordCentered& obj);
//std::wostream& operator<<(std::wostream& os, const DisplayCoordCentered& obj);

bool operator==(const DisplayDelta& lhs, const DisplayDelta& rhs);
bool operator!=(const DisplayDelta& lhs, const DisplayDelta& rhs);
//std::ostream& operator<<(std::ostream& os, const DisplayDelta& obj);
//std::wostream& operator<<(std::wostream& os, const DisplayDelta& obj);

bool operator==(const MapPixelCoord& lhs, const MapPixelCoord& rhs);
bool operator!=(const MapPixelCoord& lhs, const MapPixelCoord& rhs);
//std::ostream& operator<<(std::ostream& os, const MapPixelCoord& obj);
//std::wostream& operator<<(std::wostream& os, const MapPixelCoord& obj);

bool operator==(const MapPixelDelta& lhs, const MapPixelDelta& rhs);
bool operator!=(const MapPixelDelta& lhs, const MapPixelDelta& rhs);
//std::ostream& operator<<(std::ostream& os, const MapPixelDelta& obj);
//std::wostream& operator<<(std::wostream& os, const MapPixelDelta& obj);

bool operator==(const MapPixelCoordInt& lhs, const MapPixelCoordInt& rhs);
bool operator!=(const MapPixelCoordInt& lhs, const MapPixelCoordInt& rhs);
//std::ostream& operator<<(std::ostream& os, const MapPixelCoordInt& obj);
//std::wostream& operator<<(std::wostream& os, const MapPixelCoordInt& obj);

bool operator==(const MapPixelDeltaInt& lhs, const MapPixelDeltaInt& rhs);
bool operator!=(const MapPixelDeltaInt& lhs, const MapPixelDeltaInt& rhs);
//std::ostream& operator<<(std::ostream& os, const MapPixelDeltaInt& obj);
//std::wostream& operator<<(std::wostream& os, const MapPixelDeltaInt& obj);

bool operator==(const MapBezierGradient& lhs, const MapBezierGradient& rhs);
bool operator!=(const MapBezierGradient& lhs, const MapBezierGradient& rhs);
//std::ostream& operator<<(std::ostream& os, const MapBezierGradient& obj);
//std::wostream& operator<<(std::wostream& os, const MapBezierGradient& obj);

bool operator==(const BaseMapCoord& lhs, const BaseMapCoord& rhs);
bool operator!=(const BaseMapCoord& lhs, const BaseMapCoord& rhs);
//std::ostream& operator<<(std::ostream& os, const BaseMapCoord& obj);
//std::wostream& operator<<(std::wostream& os, const BaseMapCoord& obj);

bool operator==(const BaseMapDelta& lhs, const BaseMapDelta& rhs);
bool operator!=(const BaseMapDelta& lhs, const BaseMapDelta& rhs);
//std::ostream& operator<<(std::ostream& os, const BaseMapDelta& obj);
//std::wostream& operator<<(std::wostream& os, const BaseMapDelta& obj);


DisplayDelta operator*(DisplayDelta lhs, double rhs);
DisplayDelta operator*(double lhs, DisplayDelta rhs);
DisplayDelta operator/(DisplayDelta lhs, double rhs);
DisplayCoordCentered operator*(DisplayCoordCentered lhs, double rhs);
DisplayCoordCentered operator*(double lhs, DisplayCoordCentered rhs);
DisplayCoordCentered operator/(DisplayCoordCentered lhs, double rhs);
MapPixelDelta operator*(MapPixelDelta lhs, double rhs);
MapPixelDelta operator*(double lhs, MapPixelDelta rhs);
MapPixelDelta operator/(MapPixelDelta lhs, double rhs);
BaseMapDelta operator*(BaseMapDelta lhs, double rhs);
BaseMapDelta operator*(double lhs, BaseMapDelta rhs);
BaseMapDelta operator/(BaseMapDelta lhs, double rhs);
MapPixelDeltaInt operator*(MapPixelDeltaInt lhs, int rhs);
MapPixelDeltaInt operator*(int lhs, MapPixelDeltaInt rhs);
MapPixelDeltaInt operator/(MapPixelDeltaInt lhs, int rhs);
MapBezierGradient operator*(MapBezierGradient lhs, double rhs);
MapBezierGradient operator*(double lhs, MapBezierGradient rhs);
MapBezierGradient operator/(MapBezierGradient lhs, double rhs);

DisplayCoord operator+(DisplayCoord lhs, const DisplayDelta &rhs);
DisplayCoord operator+(const DisplayDelta &lhs, DisplayCoord rhs);
DisplayCoord operator-(DisplayCoord lhs, const DisplayDelta &rhs);
DisplayCoordCentered operator+(DisplayCoordCentered lhs, const DisplayDelta &rhs);
DisplayCoordCentered operator+(const DisplayDelta &lhs, DisplayCoordCentered rhs);
DisplayCoordCentered operator-(DisplayCoordCentered lhs, const DisplayDelta &rhs);
MapPixelCoord operator+(MapPixelCoord lhs, const MapPixelDelta &rhs);
MapPixelCoord operator+(const MapPixelDelta &lhs, MapPixelCoord rhs);
MapPixelCoord operator-(MapPixelCoord lhs, const MapPixelDelta &rhs);
MapPixelCoordInt operator+(MapPixelCoordInt lhs, const MapPixelDeltaInt &rhs);
MapPixelCoordInt operator+(const MapPixelDeltaInt &lhs, MapPixelCoordInt rhs);
MapPixelCoordInt operator-(MapPixelCoordInt lhs, const MapPixelDeltaInt &rhs);
BaseMapCoord operator+(BaseMapCoord lhs, const BaseMapDelta &rhs);
BaseMapCoord operator+(const BaseMapDelta &lhs, BaseMapCoord rhs);
BaseMapCoord operator-(BaseMapCoord lhs, const BaseMapDelta &rhs);


DisplayDelta operator+(DisplayDelta lhs, const DisplayDelta &rhs);
DisplayDelta operator-(DisplayDelta lhs, const DisplayDelta &rhs);

MapPixelDelta operator+(MapPixelDelta lhs, const MapPixelDelta &rhs);
MapPixelDelta operator-(MapPixelDelta lhs, const MapPixelDelta &rhs);

MapPixelDeltaInt operator+(MapPixelDeltaInt lhs, const MapPixelDeltaInt &rhs);
MapPixelDeltaInt operator-(MapPixelDeltaInt lhs, const MapPixelDeltaInt &rhs);

LatLonDelta operator+(LatLonDelta lhs, const LatLonDelta &rhs);
LatLonDelta operator-(LatLonDelta lhs, const LatLonDelta &rhs);

BaseMapDelta operator+(BaseMapDelta lhs, const BaseMapDelta &rhs);
BaseMapDelta operator-(BaseMapDelta lhs, const BaseMapDelta &rhs);



DisplayDelta
operator-(const DisplayCoord &lhs, const DisplayCoord &rhs);

DisplayDelta
operator-(const DisplayCoordCentered &lhs, const DisplayCoordCentered &rhs);

MapPixelDelta
operator-(const MapPixelCoord &lhs, const MapPixelCoord &rhs);

MapPixelDeltaInt
operator-(const MapPixelCoordInt &lhs, const MapPixelCoordInt &rhs);

BaseMapDelta
operator-(const BaseMapCoord &lhs, const BaseMapCoord &rhs);

MapPixelDelta
operator*(const MapPixelDeltaInt &lhs, double rhs);

MapPixelDelta
operator*(double lhs, const MapPixelDeltaInt &rhs);

MapPixelDelta
operator/(const MapPixelDeltaInt &lhs, double rhs);



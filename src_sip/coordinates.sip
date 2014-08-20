%ModuleHeaderCode
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"

PyObject *repr_helper(PyObject *obj, double f1, double f2);
%End

%ModuleCode
PyObject *repr_helper(PyObject *obj, double f1, double f2) {
    PyObject *o1 = PyFloat_FromDouble(f1);
    PyObject *o2 = PyFloat_FromDouble(f2);
    PyObject *result = nullptr;
    if (o1 && o2) {
        result = PyUnicode_FromFormat("<%s(%R, %R)>",
                                      obj->ob_type->tp_name, o1, o2);
    } else {
        PyErr_Format(PyExc_ValueError,
                     "Could not get __repr__ for type");
    }
    Py_XDECREF(o1);
    Py_XDECREF(o2);
    return result;
}
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
};

class PixelBufCoord {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        PixelBufCoord();
        PixelBufCoord(int x_, int y_);

        PixelBufCoord& operator+=(const PixelBufDelta &rhs);
        PixelBufCoord& operator-=(const PixelBufDelta &rhs);
        int x;
        int y;

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = PyUnicode_FromFormat("<%s(%d, %d)>",
                                          sipSelf->ob_type->tp_name,
                                          sipCpp->x, sipCpp->y);
        %End
};

class PixelBufDelta {
%TypeHeaderCode
#include "coordinates.h"
%End
    public:
        PixelBufDelta();
        PixelBufDelta(int x_, int y_);
        PixelBufDelta& operator+=(const PixelBufDelta &rhs);
        PixelBufDelta& operator-=(const PixelBufDelta &rhs);

        int x;
        int y;

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = PyUnicode_FromFormat("<%s(%d, %d)>",
                                          sipSelf->ob_type->tp_name,
                                          sipCpp->x, sipCpp->y);
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = PyUnicode_FromFormat("<%s(%d, %d)>",
                                          sipSelf->ob_type->tp_name,
                                          sipCpp->x, sipCpp->y);
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = PyUnicode_FromFormat("<%s(%d, %d)>",
                                          sipSelf->ob_type->tp_name,
                                          sipCpp->x, sipCpp->y);
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->lat, sipCpp->lon);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->lat, sipCpp->lon);
            if (!sipRes)
                sipIsErr = 1;
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            PyObject *x = PyFloat_FromDouble(sipCpp->x);
            PyObject *y = PyFloat_FromDouble(sipCpp->y);
            if (x && y) {
                sipRes = PyUnicode_FromFormat("<%s(%d%c %R %R)>",
                                              sipSelf->ob_type->tp_name,
                                              sipCpp->zone,
                                              sipCpp->northp ? 'N' : 'S',
                                              x, y);
            } else {
                PyErr_Format(PyExc_ValueError,
                             "Could not get __repr__ for type");
                sipIsErr = 1;
            }
            Py_XDECREF(x);
            Py_XDECREF(y);
        %End
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

        SIP_PYOBJECT __repr__();
        %MethodCode
            sipRes = repr_helper(sipSelf, sipCpp->x, sipCpp->y);
            if (!sipRes)
                sipIsErr = 1;
        %End
};


bool operator==(const PixelBufCoord& lhs, const PixelBufCoord& rhs);
bool operator!=(const PixelBufCoord& lhs, const PixelBufCoord& rhs);
bool operator==(const PixelBufDelta& lhs, const PixelBufDelta& rhs);
bool operator!=(const PixelBufDelta& lhs, const PixelBufDelta& rhs);

bool operator==(const DisplayCoord& lhs, const DisplayCoord& rhs);
bool operator!=(const DisplayCoord& lhs, const DisplayCoord& rhs);
bool operator==(const DisplayCoordCentered& lhs, const DisplayCoordCentered& rhs);
bool operator!=(const DisplayCoordCentered& lhs, const DisplayCoordCentered& rhs);
bool operator==(const DisplayDelta& lhs, const DisplayDelta& rhs);
bool operator!=(const DisplayDelta& lhs, const DisplayDelta& rhs);

bool operator==(const MapPixelCoord& lhs, const MapPixelCoord& rhs);
bool operator!=(const MapPixelCoord& lhs, const MapPixelCoord& rhs);
bool operator==(const MapPixelDelta& lhs, const MapPixelDelta& rhs);
bool operator!=(const MapPixelDelta& lhs, const MapPixelDelta& rhs);

bool operator==(const MapPixelCoordInt& lhs, const MapPixelCoordInt& rhs);
bool operator!=(const MapPixelCoordInt& lhs, const MapPixelCoordInt& rhs);
bool operator==(const MapPixelDeltaInt& lhs, const MapPixelDeltaInt& rhs);
bool operator!=(const MapPixelDeltaInt& lhs, const MapPixelDeltaInt& rhs);

bool operator==(const MapBezierGradient& lhs, const MapBezierGradient& rhs);
bool operator!=(const MapBezierGradient& lhs, const MapBezierGradient& rhs);

bool operator==(const BaseMapCoord& lhs, const BaseMapCoord& rhs);
bool operator!=(const BaseMapCoord& lhs, const BaseMapCoord& rhs);

bool operator==(const BaseMapDelta& lhs, const BaseMapDelta& rhs);
bool operator!=(const BaseMapDelta& lhs, const BaseMapDelta& rhs);


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

PixelBufCoord operator+(PixelBufCoord lhs, const PixelBufDelta &rhs);
PixelBufCoord operator+(const PixelBufDelta &lhs, PixelBufCoord rhs);
PixelBufCoord operator-(PixelBufCoord lhs, const PixelBufDelta &rhs);
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

PixelBufDelta operator+(PixelBufDelta lhs, const PixelBufDelta &rhs);
PixelBufDelta operator-(PixelBufDelta lhs, const PixelBufDelta &rhs);

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



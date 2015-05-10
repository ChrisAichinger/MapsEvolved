// Copyright 2015 Christian Aichinger <Greek0@gmx.net>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <iostream>

#include "../include/rastermap.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(rastermap)

static std::wstring empty_wstr(L"");

class MockGeoDrawable : public GeoDrawable {
public:
    virtual ~MockGeoDrawable() {};
    virtual bool
    PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
        return false;
    }
    virtual bool
    LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
        return false;
    }

    virtual DrawableType GetType() const { return TYPE_MAP; }
    virtual unsigned int GetWidth() const { return 64; }
    virtual unsigned int GetHeight() const { return 64; }
    virtual MapPixelDeltaInt GetSize() const {
        return MapPixelDeltaInt(64, 64);
    }

    virtual PixelBuf GetRegion(const MapPixelCoordInt &pos,
                               const MapPixelDeltaInt &size) const
    {
        return PixelBuf();
    }

    virtual Projection GetProj() const { return Projection(""); }
    virtual const std::wstring &GetFname() const { return empty_wstr; }
    virtual const std::wstring &GetTitle() const { return empty_wstr; }
    virtual const std::wstring &GetDescription() const { return empty_wstr; }

    virtual ODMPixelFormat GetPixelFormat() const { return ODM_PIX_RGBA4; }
};

class LatLonTransGeoDrawable : public GeoPixels {
public:
    LatLonTransGeoDrawable(double factor_x, double factor_y,
                           bool fail_p2l, bool fail_l2p)
        : m_factor_x(factor_x), m_factor_y(factor_y),
          m_fail_p2l(fail_p2l), m_fail_l2p(fail_l2p)
    {}
    virtual ~LatLonTransGeoDrawable() {};
    virtual bool
    PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
        result->lat = pos.x * m_factor_x;
        result->lon = pos.y * m_factor_y;
        return !m_fail_p2l;
    }
    virtual bool
    LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
        result->x = pos.lat / m_factor_x;
        result->y = pos.lon / m_factor_y;
        return !m_fail_l2p;
    }
private:
    double m_factor_x, m_factor_y;
    bool m_fail_p2l, m_fail_l2p;
};

#define CHECK_COORD_CLOSE(lhs, rhs, percent)                           \
    do {                                                               \
        auto &lhs_ = (lhs);                                            \
        auto &rhs_ = (rhs);                                            \
        BOOST_CHECK_CLOSE(lhs_.x, rhs_.x, percent);                    \
        BOOST_CHECK_CLOSE(lhs_.y, rhs_.y, percent);                    \
    } while (0)


BOOST_AUTO_TEST_CASE(MapCoordToMapCoord)
{
    auto coord_in =  MapPixelCoord(4, 6);
    auto coord_out = MapPixelCoord(4.0 * 100 / 300, 6.0 * 200 / 500);

    auto map_from = LatLonTransGeoDrawable(100, 200, false, false);
    auto map_to =   LatLonTransGeoDrawable(300, 500, false, false);
    auto map_fail_p2l = LatLonTransGeoDrawable(300, 500, true, false);
    auto map_fail_l2p = LatLonTransGeoDrawable(100, 200, false, true);

    CHECK_COORD_CLOSE(MapPixelToMapPixel(coord_in, map_from,     map_to),
                      coord_out, 0.1);
    CHECK_COORD_CLOSE(MapPixelToMapPixel(coord_in, map_from,     map_fail_p2l),
                      coord_out, 0.1);
    CHECK_COORD_CLOSE(MapPixelToMapPixel(coord_in, map_fail_l2p, map_to),
                      coord_out, 0.1);

    BOOST_CHECK_THROW(MapPixelToMapPixel(coord_in, map_fail_p2l, map_to),
                      std::runtime_error);
    BOOST_CHECK_THROW(MapPixelToMapPixel(coord_in, map_from,     map_fail_l2p),
                      std::runtime_error);
}


BOOST_AUTO_TEST_SUITE_END()

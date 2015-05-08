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

#include <stdio.h>
#include <string>
#include <cassert>

#include "../include/coordinates.h"
#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/pair.hpp>

BOOST_AUTO_TEST_SUITE(coords)

typedef boost::mpl::list<DisplayCoord,
                         DisplayCoordCentered,
                         DisplayDelta,
                         MapPixelCoord,
                         MapPixelDelta,
                         MapBezierGradient,
                         BaseMapCoord,
                         BaseMapDelta
                        > value_eq_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(value_eq, T, value_eq_types)
{
    BOOST_CHECK_EQUAL(sizeof(T), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(T().x, 0);
    BOOST_CHECK_EQUAL(T().y, 0);
    BOOST_CHECK_EQUAL(T(0.5, 16.5).x,  0.5);
    BOOST_CHECK_EQUAL(T(0.5, 16.5).y, 16.5);
    BOOST_CHECK_EQUAL(T(0.5, 16.5),  T(0.5, 16.5));
    BOOST_CHECK_NE(T(0.51, 16.5),    T(0.5, 16.5));
    BOOST_CHECK_NE(T(0.5, 16.49),    T(0.5, 16.5));
}

BOOST_AUTO_TEST_CASE(value_eq_latlon)
{
    BOOST_CHECK_EQUAL(sizeof(LatLon), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(LatLon().lat, 0);
    BOOST_CHECK_EQUAL(LatLon().lon, 0);
    BOOST_CHECK_EQUAL(LatLon(0.5, 16.5).lat,  0.5);
    BOOST_CHECK_EQUAL(LatLon(0.5, 16.5).lon, 16.5);
    //BOOST_CHECK_EQUAL(LatLon(0.5, 16.5), LatLon(0.5, 16.5));
    //BOOST_CHECK_NE(LatLon(0.51, 16.5),   LatLon(0.5, 16.5));
    //BOOST_CHECK_NE(LatLon(0.5, 16.49),   LatLon(0.5, 16.5));
}

BOOST_AUTO_TEST_CASE(value_eq_utmups)
{
    BOOST_CHECK_EQUAL(UTMUPS().zone, 0);
    BOOST_CHECK_EQUAL(UTMUPS().x, 0);
    BOOST_CHECK_EQUAL(UTMUPS().y, 0);

    BOOST_CHECK_EQUAL(UTMUPS(32, true,  0.5, 16.5).zone,     32);
    BOOST_CHECK_EQUAL(UTMUPS(32, false, 0.5, 16.5).zone,     32);
    BOOST_CHECK_EQUAL(UTMUPS(32, true,  0.5, 16.5).northp, true);
    BOOST_CHECK_EQUAL(UTMUPS(32, true,  0.5, 16.5).x,       0.5);
    BOOST_CHECK_EQUAL(UTMUPS(32, false, 0.5, 16.5).y,      16.5);

    //BOOST_CHECK_EQUAL(UTMUPS(32, false, 0.5, 16.5), UTMUPS(32, false, 0.5, 16.5));
    //BOOST_CHECK_EQUAL(UTMUPS(32,  true, 0.5, 16.5), UTMUPS(32,  true, 0.5, 16.5));
    //
    //BOOST_CHECK_NE(UTMUPS(33,  true, 0.5, 16.5), UTMUPS(32,  true, 0.5, 16.5));
    //BOOST_CHECK_NE(UTMUPS(32, false, 0.5, 16.5), UTMUPS(32,  true, 0.5, 16.5));
    //BOOST_CHECK_NE(UTMUPS(32,  true, 0.8, 16.5), UTMUPS(32,  true, 0.5, 16.5));
    //BOOST_CHECK_NE(UTMUPS(32,  true, 0.5, 16.0), UTMUPS(32,  true, 0.5, 16.5));
}

typedef boost::mpl::list<PixelBufCoord,
                         PixelBufDelta,
                         MapPixelCoordInt,
                         MapPixelDeltaInt
                        > value_eq_types_int;

BOOST_AUTO_TEST_CASE_TEMPLATE(value_eq_int, T, value_eq_types_int)
{
    BOOST_CHECK_EQUAL(sizeof(T), 2 * sizeof(int));
    BOOST_CHECK_EQUAL(T().x, 0);
    BOOST_CHECK_EQUAL(T().y, 0);
    BOOST_CHECK_EQUAL(T(2, 16).x,  2);
    BOOST_CHECK_EQUAL(T(2, 16).y, 16);
    BOOST_CHECK_EQUAL(T(2, 16),  T(2, 16));
    BOOST_CHECK_NE(T(3, 16),     T(2, 16));
    BOOST_CHECK_NE(T(2, 15),     T(2, 16));
}


typedef boost::mpl::list<boost::mpl::pair<PixelBufCoord,        PixelBufDelta>,
                         boost::mpl::pair<DisplayCoord,         DisplayDelta>,
                         boost::mpl::pair<DisplayCoordCentered, DisplayDelta>,
                         boost::mpl::pair<MapPixelCoord,        MapPixelDelta>,
                         boost::mpl::pair<MapPixelCoordInt,     MapPixelDeltaInt>,
                         boost::mpl::pair<BaseMapCoord,         BaseMapDelta>,

                         boost::mpl::pair<PixelBufDelta,    PixelBufDelta>,
                         boost::mpl::pair<DisplayDelta,     DisplayDelta>,
                         boost::mpl::pair<MapPixelDelta,    MapPixelDelta>,
                         boost::mpl::pair<MapPixelDeltaInt, MapPixelDeltaInt>,
                         boost::mpl::pair<BaseMapDelta,     BaseMapDelta>
                        > addsub_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(addsub, T, addsub_types)
{
    typedef T::first Coord;
    typedef T::second Delta;

    Delta DD_5_15(5, 15);
    BOOST_CHECK_EQUAL(Coord(10, 20) += DD_5_15, Coord(15, 35));
    BOOST_CHECK_EQUAL(Coord(30, 20) -= DD_5_15, Coord(25, 5));

    BOOST_CHECK_EQUAL(Coord(10, 20) + DD_5_15, Coord(15, 35));
    BOOST_CHECK_EQUAL(DD_5_15 + Coord(10, 20), Coord(15, 35));
    BOOST_CHECK_EQUAL(Coord(30, 20) - DD_5_15, Coord(25, 5));
}


typedef boost::mpl::list<DisplayDelta,
                         DisplayCoordCentered,
                         MapPixelDelta,
                         BaseMapDelta,
                         MapBezierGradient
                        > muldiv_float_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(muldiv_float, T, muldiv_float_types)
{
    BOOST_CHECK_EQUAL(T(10, 20) *  1.5, T(15,       30));
    BOOST_CHECK_EQUAL(T(10, 20) *= 1.5, T(15,       30));
    BOOST_CHECK_EQUAL(T(10, 20) /  1.5, T( 20./3, 40./3));
    BOOST_CHECK_EQUAL(T(10, 20) /= 1.5, T( 20./3, 40./3));
}

typedef boost::mpl::list<MapPixelDeltaInt
                        > muldiv_int_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(muldiv_int, T, muldiv_int_types)
{
    BOOST_CHECK_EQUAL(T(8, 32) *  2, T(16, 64));
    BOOST_CHECK_EQUAL(T(8, 32) *= 2, T(16, 64));
    BOOST_CHECK_EQUAL(T(8, 32) /  2, T(4, 16));
    BOOST_CHECK_EQUAL(T(8, 32) /= 2, T(4, 16));
}


typedef boost::mpl::list<boost::mpl::pair<DisplayCoord, DisplayDelta>,
                         boost::mpl::pair<DisplayCoordCentered, DisplayDelta>,
                         boost::mpl::pair<MapPixelCoord, MapPixelDelta>,
                         boost::mpl::pair<MapPixelCoordInt, MapPixelDeltaInt>,
                         boost::mpl::pair<BaseMapCoord, BaseMapDelta>
                        > coord_diff_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(coord_diff, T, coord_diff_types)
{
    typedef T::first Coord;
    typedef T::second Delta;

    BOOST_CHECK_EQUAL(Coord(8, 32) - Coord(  2,  4), Delta( 6, 28));
    BOOST_CHECK_EQUAL(Coord(8, 32) - Coord( 16,  4), Delta(-8, 28));
    BOOST_CHECK_EQUAL(Coord(8, 32) - Coord(-16, -4), Delta(24, 36));
}


BOOST_AUTO_TEST_CASE(mpdi_muldiv)
{
    BOOST_CHECK_EQUAL(1.5 * MapPixelDeltaInt(8, 32), MapPixelDelta(12, 48));
    BOOST_CHECK_EQUAL(MapPixelDeltaInt(8, 32) * 1.5, MapPixelDelta(12, 48));
    BOOST_CHECK_EQUAL(MapPixelDeltaInt(12, 48) / 1.5, MapPixelDelta(8, 32));
}


BOOST_AUTO_TEST_SUITE_END()

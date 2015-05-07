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
                         DisplayDelta
                        >
        value_eq_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(value_eq, T, value_eq_types)
{
    BOOST_CHECK_EQUAL(sizeof(T), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(T().x, 0);
    BOOST_CHECK_EQUAL(T().y, 0);
    BOOST_CHECK_EQUAL(T(0.5, 16.5).x,  0.5);
    BOOST_CHECK_EQUAL(T(0.5, 16.5).y, 16.5);
    BOOST_CHECK_EQUAL(T(0.5, 16.5), T(0.5, 16.5));
    BOOST_CHECK_NE(T(0.51, 16.5),   T(0.5, 16.5));
    BOOST_CHECK_NE(T(0.5, 16.49),   T(0.5, 16.5));
}

typedef boost::mpl::list<boost::mpl::pair<DisplayCoord, DisplayDelta>,
                         boost::mpl::pair<DisplayCoordCentered, DisplayDelta>
                        >
        addsub_types;

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

typedef boost::mpl::list<DisplayCoordCentered,
                         DisplayDelta
                        >
        muldiv_int_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(muldiv_int, T, muldiv_int_types)
{
    BOOST_CHECK_EQUAL(T(10, 20) *= 2, T(20, 40));
    BOOST_CHECK_EQUAL(T(10, 20) /= 2, T(5, 10));
    BOOST_CHECK_EQUAL(T(10, 20) * 2, T(20, 40));
    BOOST_CHECK_EQUAL(T(10, 20) / 2, T(5, 10));
}

BOOST_AUTO_TEST_SUITE_END()
#include <stdio.h>
#include <string>
#include <cassert>

#include "../include/coordinates.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(coords)
{
    BOOST_CHECK_EQUAL(sizeof(DisplayCoord), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(DisplayCoord().x, 0);
    BOOST_CHECK_EQUAL(DisplayCoord().y, 0);
    BOOST_CHECK_EQUAL(DisplayCoord(10, 20).x, 10);
    BOOST_CHECK_EQUAL(DisplayCoord(10, 20).y, 20);

    BOOST_CHECK_EQUAL(sizeof(DisplayCoordCentered), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(DisplayCoordCentered().x, 0);
    BOOST_CHECK_EQUAL(DisplayCoordCentered().y, 0);
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20).x, 10);
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20).y, 20);

    BOOST_CHECK_EQUAL(sizeof(DisplayDelta), 2 * sizeof(double));
    BOOST_CHECK_EQUAL(DisplayDelta().x, 0);
    BOOST_CHECK_EQUAL(DisplayDelta().y, 0);
    BOOST_CHECK_EQUAL(DisplayDelta(10, 20).x, 10);
    BOOST_CHECK_EQUAL(DisplayDelta(10, 20).y, 20);

    BOOST_CHECK_EQUAL(DisplayCoord(10, 20), DisplayCoord(10, 20));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20), DisplayCoordCentered(10, 20));
    BOOST_CHECK_EQUAL(DisplayDelta(10, 20), DisplayDelta(10, 20));

    BOOST_CHECK_NE(DisplayCoord(10.01, 20), DisplayCoord(10, 20));
    BOOST_CHECK_NE(DisplayCoordCentered(10.01, 20), DisplayCoordCentered(10, 20));
    BOOST_CHECK_NE(DisplayDelta(10.01, 20), DisplayDelta(10, 20));

    BOOST_CHECK_NE(DisplayCoord(10, 19.99), DisplayCoord(10, 20));
    BOOST_CHECK_NE(DisplayCoordCentered(10, 19.99), DisplayCoordCentered(10, 20));
    BOOST_CHECK_NE(DisplayDelta(10, 19.99), DisplayDelta(10, 20));

    DisplayDelta DD_5_15(5, 15);
    BOOST_CHECK_EQUAL(DisplayCoord(10, 20) += DD_5_15,
            DisplayCoord(15, 35));
    BOOST_CHECK_EQUAL(DisplayCoord(30, 20) -= DD_5_15,
            DisplayCoord(25, 5));

    BOOST_CHECK_EQUAL(DisplayCoord(10, 20) + DD_5_15,
            DisplayCoord(15, 35));
    BOOST_CHECK_EQUAL(DD_5_15 + DisplayCoord(10, 20),
            DisplayCoord(15, 35));
    BOOST_CHECK_EQUAL(DisplayCoord(30, 20) - DD_5_15,
            DisplayCoord(25, 5));

    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) += DD_5_15,
            DisplayCoordCentered(15, 35));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(30, 20) -= DD_5_15,
            DisplayCoordCentered(25, 5));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) *= 2,
            DisplayCoordCentered(20, 40));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) /= 2,
            DisplayCoordCentered(5, 10));

    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) + DD_5_15,
            DisplayCoordCentered(15, 35));
    BOOST_CHECK_EQUAL(DD_5_15 + DisplayCoordCentered(10, 20),
            DisplayCoordCentered(15, 35));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(30, 20) - DD_5_15,
            DisplayCoordCentered(25, 5));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) * 2,
            DisplayCoordCentered(20, 40));
    BOOST_CHECK_EQUAL(DisplayCoordCentered(10, 20) / 2,
            DisplayCoordCentered(5, 10));
}

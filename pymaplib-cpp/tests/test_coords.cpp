#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>

#include "coordinates.h"
#include "test.h"

int test_coords() {
    test_eq(sizeof(DisplayCoord), 2 * sizeof(double));
    test_eq(DisplayCoord().x, 0);
    test_eq(DisplayCoord().y, 0);
    test_eq(DisplayCoord(10, 20).x, 10);
    test_eq(DisplayCoord(10, 20).y, 20);

    test_eq(sizeof(DisplayCoordCentered), 2 * sizeof(double));
    test_eq(DisplayCoordCentered().x, 0);
    test_eq(DisplayCoordCentered().y, 0);
    test_eq(DisplayCoordCentered(10, 20).x, 10);
    test_eq(DisplayCoordCentered(10, 20).y, 20);

    test_eq(sizeof(DisplayDelta), 2 * sizeof(double));
    test_eq(DisplayDelta().x, 0);
    test_eq(DisplayDelta().y, 0);
    test_eq(DisplayDelta(10, 20).x, 10);
    test_eq(DisplayDelta(10, 20).y, 20);

    test_eq(DisplayCoord(10, 20), DisplayCoord(10, 20));
    test_eq(DisplayCoordCentered(10, 20), DisplayCoordCentered(10, 20));
    test_eq(DisplayDelta(10, 20), DisplayDelta(10, 20));

    test_neq(DisplayCoord(10.01, 20), DisplayCoord(10, 20));
    test_neq(DisplayCoordCentered(10.01, 20), DisplayCoordCentered(10, 20));
    test_neq(DisplayDelta(10.01, 20), DisplayDelta(10, 20));

    test_neq(DisplayCoord(10, 19.99), DisplayCoord(10, 20));
    test_neq(DisplayCoordCentered(10, 19.99), DisplayCoordCentered(10, 20));
    test_neq(DisplayDelta(10, 19.99), DisplayDelta(10, 20));

    DisplayDelta DD_5_15(5, 15);
    test_eq(DisplayCoord(10, 20) += DD_5_15,
            DisplayCoord(15, 35));
    test_eq(DisplayCoord(30, 20) -= DD_5_15,
            DisplayCoord(25, 5));

    test_eq(DisplayCoord(10, 20) + DD_5_15,
            DisplayCoord(15, 35));
    test_eq(DD_5_15 + DisplayCoord(10, 20),
            DisplayCoord(15, 35));
    test_eq(DisplayCoord(30, 20) - DD_5_15,
            DisplayCoord(25, 5));

    test_eq(DisplayCoordCentered(10, 20) += DD_5_15,
            DisplayCoordCentered(15, 35));
    test_eq(DisplayCoordCentered(30, 20) -= DD_5_15,
            DisplayCoordCentered(25, 5));
    test_eq(DisplayCoordCentered(10, 20) *= 2,
            DisplayCoordCentered(20, 40));
    test_eq(DisplayCoordCentered(10, 20) /= 2,
            DisplayCoordCentered(5, 10));

    test_eq(DisplayCoordCentered(10, 20) + DD_5_15,
            DisplayCoordCentered(15, 35));
    test_eq(DD_5_15 + DisplayCoordCentered(10, 20),
            DisplayCoordCentered(15, 35));
    test_eq(DisplayCoordCentered(30, 20) - DD_5_15,
            DisplayCoordCentered(25, 5));
    test_eq(DisplayCoordCentered(10, 20) * 2,
            DisplayCoordCentered(20, 40));
    test_eq(DisplayCoordCentered(10, 20) / 2,
            DisplayCoordCentered(5, 10));
    return 0;
}

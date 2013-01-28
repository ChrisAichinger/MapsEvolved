#define _USE_MATH_DEFINES
#include <memory>
#include <algorithm>
#include <cassert>
#include <cmath>

#include "util.h"
#include "bezier.h"
#include "rastermap.h"

unsigned int factorial(unsigned int n)
{
    unsigned char cache[] = { 1, 1, 2, 6, 24, 120 };
    if (n < ARRAY_SIZE(cache))
        return cache[n];

    unsigned int ret = 1;
    for(unsigned int i = 1; i <= n; ++i)
        ret *= i;
    return ret;
}

inline unsigned int binomial(unsigned int n, int k) {
    if (k < 0 || (unsigned int)k > n)
        return 0;

    static const unsigned int cache_size = 3;
    unsigned char cache[cache_size][cache_size] = { { 1, 0, 0 },
                                                    { 1, 1, 0 },
                                                    { 1, 2, 1 } };
    if (n < cache_size)
        return cache[n][k];

    return factorial(n) / (factorial(k) * factorial(n-k));
}



Bezier::Bezier(const FromControlPoints &points)
{
    memcpy(m_points, points.Get(), sizeof(m_points));
};

Bezier::Bezier(const FitData &points)
{
    memcpy(m_points, points.Get(), sizeof(m_points));
    DoFitData();
}

#define MP(xx,yy) m_points[(xx) + N_POINTS * (yy)]
void Bezier::DoFitData() {
    MP(0,1) = 2 * MP(0,1) - 0.5 * (MP(0,0) + MP(0,2));
    MP(2,1) = 2 * MP(2,1) - 0.5 * (MP(2,0) + MP(2,2));
    MP(1,0) = 2 * MP(1,0) - 0.5 * (MP(0,0) + MP(2,0));
    MP(1,2) = 2 * MP(1,2) - 0.5 * (MP(0,2) + MP(2,2));
    MP(1,1) = 4 * MP(1,1)
                  - 0.25 * (MP(0,0) + MP(0,2) + MP(2,0) + MP(2,2))
                  - 0.50 * (MP(1,0) + MP(0,1) + MP(1,2) + MP(2,1));
}

double Bezier::GetValue(double x, double y) {
    double bx[N_POINTS];
    double by[N_POINTS];
    Bernstein_vec(N_POINTS - 1, x, bx);
    Bernstein_vec(N_POINTS - 1, y, by);
    double sum = 0.0;
    for (unsigned int i=0; i < N_POINTS; i++) {
        for (unsigned int j=0; j < N_POINTS; j++) {
            sum += bx[i] * MP(i, j) * by[j];
        }
    }
    return sum;
}

void Bezier::GetGradient(double *x, double *y) {
    double bx[N_POINTS], by[N_POINTS];
    double bx_deriv[N_POINTS], by_deriv[N_POINTS];
    Bernstein_vec(N_POINTS - 1, *x, bx);
    Bernstein_vec(N_POINTS - 1, *y, by);
    Bernstein_deriv_vec(N_POINTS - 1, *x, bx_deriv);
    Bernstein_deriv_vec(N_POINTS - 1, *y, by_deriv);
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (unsigned int i=0; i < N_POINTS; i++) {
        for (unsigned int j=0; j < N_POINTS; j++) {
            sum_x += bx_deriv[i] * MP(i, j) * by[j];
            sum_y += bx[i] * MP(i, j) * by_deriv[j];
        }
    }
    *x = sum_x;
    *y = sum_y;
}

void Bezier::Bernstein_vec(unsigned int degree, double x, double *out) {
    assert(degree = 2);
    if (x == 0.0) {
        out[0] = 1; out[1] = 0; out[2] = 0;
        return;
    } else if (x == 1.0) {
        out[0] = 0; out[1] = 0; out[2] = 1;
        return;
    } else if (x == 0.5) {
        out[0] = 0.25; out[1] = 0.5; out[2] = 0.25;
        return;
    }
    for (unsigned int i=0; i <= degree; i++) {
        out[i] = Bernstein(degree, i, x);
    }
}

void Bezier::Bernstein_deriv_vec(unsigned int degree, double x, double *out) {
    assert(degree = 2);
    if (x == 0.0) {
        out[0] = -2; out[1] = 2; out[2] = 0;
        return;
    } else if (x == 1.0) {
        out[0] = 0; out[1] = -2; out[2] = 2;
        return;
    } else if (x == 0.5) {
        out[0] = -1; out[1] = 0; out[2] = 1;
        return;
    }
    for (unsigned int i=0; i <= degree; i++) {
        out[i] = degree * (Bernstein(degree - 1, i - 1, x) -
                           Bernstein(degree - 1, i, x));
    }
}

double Bezier::Bernstein(unsigned int degree, unsigned int v, double x) {
    return binomial(degree, v) * pow(x, (int)v) * pow(1-x, (int)(degree - v));
}

MapBezier::MapBezier(const RasterMap &map, unsigned int x, unsigned int y) {
    assert(x > 0 && y > 0 && x < map.GetWidth()-1 && y < map.GetHeight()-1);

    m_center_x = x;
    m_center_y = y;
    unsigned int center = (N_POINTS - 1) / 2;
    auto orig_data = map.GetRegion(x - center, y - center,
                                   N_POINTS, N_POINTS);
    InitPoints(orig_data.get(), N_POINTS, N_POINTS, center, center, true);
    DoFitData();
}

MapBezier::MapBezier(const unsigned int *src,
                     unsigned int width, unsigned int height,
                     unsigned int x, unsigned int y)
{
    m_center_x = x;
    m_center_y = y;
    InitPoints(src, width, height, x, y);
    DoFitData();
}

MapBezier::MapBezier(const class RasterMap &map, double *x, double *y) {
    unsigned int x_int, y_int;
    PointFromDouble(x, y, &x_int, &y_int, map.GetWidth(), map.GetHeight());
    assert(x_int > 0 && y_int > 0
           && x_int < map.GetWidth() - 1 && y_int < map.GetHeight() - 1);

    m_center_x = x_int;
    m_center_y = y_int;
    unsigned int center = (N_POINTS - 1) / 2;
    auto orig_data = map.GetRegion(x_int - center, y_int - center,
                                   N_POINTS, N_POINTS);
    InitPoints(orig_data.get(), N_POINTS, N_POINTS, center, center, true);
    DoFitData();
}

MapBezier::MapBezier(const unsigned int *src,
                     unsigned int width, unsigned int height,
                     double *x, double *y)
{
    unsigned int x_int, y_int;
    PointFromDouble(x, y, &x_int, &y_int, width, height);
    m_center_x = x_int;
    m_center_y = y_int;
    InitPoints(src, width, height, x_int, y_int);
    DoFitData();
}

#define SRC(xx,yy) src[(xx) + width * (yy)]
void MapBezier::InitPoints(const unsigned int *src,
                           unsigned int width, unsigned int height,
                           unsigned int x, unsigned int y,
                           bool invert_y)
{
    assert(x > 0 && y > 0 && x < width - 1 && y < height - 1);

    int sign = invert_y ? (-1) : (+1);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            MP(dx+1, dy+1) = SRC(x + dx, y + dy*sign);
        }
    }
}
#undef SRC

// Given floating point map coordinates *x and *y, find the closest point to
// create a Bezier surface (returned in *x_int, *y_int). Handles border cases
// as well.  *x and *y are set to the original coordinates in the Bezier space.
//
// INPUT       | OUTPUT
// *x    *y    | *x_int  *y_int  *x    *y
// 3.8   8.3   | 4       8       0.4   0.65
// 0.1   1.2   | 1       1       0.05  0.6
void MapBezier::PointFromDouble(double *x, double *y,
                                unsigned int *x_int, unsigned int *y_int,
                                unsigned int width, unsigned int height)
{
    assert(*x >= 0 && *y >= 0 && *x < width && *y < height);

    // Find the closest integer point
    // Cast is harmless as we clamp in the next step
    *x_int = static_cast<unsigned int>(round_to_int(*x));
    *y_int = static_cast<unsigned int>(round_to_int(*y));

    // Clamp the integer result so the sampling area is within src
    // src at the edges (e.g. x=0.1, y=width-1.1 -> x_int=1, y_int = width - 2
    *x_int = max(min(*x_int, width - 2), 1);
    *y_int = max(min(*y_int, height - 2), 1);

    // Output the Bezier coordinates of the x/y input back into *x, *y
    *x = 0.5 * (*x - *x_int + 1);
    *y = 0.5 * (*y - *y_int + 1);
}
#undef MP


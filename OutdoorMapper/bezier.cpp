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

MapBezier::MapBezier(const RasterMap &map, const MapPixelCoordInt &pos)
    : m_center_int(pos), m_creation_pos(0.5, 0.5)
{
    assert(pos.x > 0 && pos.y > 0 &&
           static_cast<unsigned int>(pos.x) < map.GetWidth() - 1 &&
           static_cast<unsigned int>(pos.y) < map.GetHeight() - 1);

    InitPoints(map, m_center_int);
    DoFitData();
}

MapBezier::MapBezier(const unsigned int *src,
                     const MapPixelCoordInt &pos, const MapPixelDeltaInt &size)
    : m_center_int(pos), m_creation_pos(0.5, 0.5)
{
    InitPoints(src, m_center_int, size, false);
    DoFitData();
}

MapBezier::MapBezier(const class RasterMap &map, const MapPixelCoord &pos)
    : m_center_int(FindCenter(pos, map.GetSize())),
      m_creation_pos(FindCreationPos(pos, m_center_int))
{
    InitPoints(map, m_center_int);
    DoFitData();
}

MapBezier::MapBezier(const unsigned int *src,
                     const MapPixelCoord &pos, const MapPixelDeltaInt &size)
    : m_center_int(FindCenter(pos, size)),
      m_creation_pos(FindCreationPos(pos, m_center_int))
{
    InitPoints(src, m_center_int, size, false);
    DoFitData();
}

#define SRC(xx,yy) src[(xx) + size.x * (yy)]
void MapBezier::InitPoints(const unsigned int *src,
                           const MapPixelCoordInt &pos,
                           const MapPixelDeltaInt &size,
                           bool invert_y)
{
    assert(pos.x > 0 && pos.y > 0 && pos.x < size.x - 1 && pos.y < size.y - 1);

    int sign = invert_y ? (-1) : (+1);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            MP(dx+1, dy+1) = SRC(pos.x + dx, pos.y + dy*sign);
        }
    }
}
#undef SRC
#undef MP

void MapBezier::InitPoints(const class RasterMap &map,
                           const MapPixelCoordInt &pos)
{
    MapPixelDeltaInt center((N_POINTS - 1) / 2, (N_POINTS - 1) / 2);
    MapPixelDeltaInt bez_size(N_POINTS, N_POINTS);
    auto orig_data = map.GetRegion(pos - center, bez_size);
    InitPoints(orig_data.get(), MapPixelCoordInt(center), bez_size, true);
}

// Turn floating point map coordinates into an integer center for Bezier
MapPixelCoordInt MapBezier::FindCenter(const MapPixelCoord &pos,
                                       const MapPixelDeltaInt &size) const
{
    // Clamp to make sure we don't sample outside of the source
    // +1 in off_max because ClampToRect uses inclusive upper bound
    const int sampling_overhang = (N_POINTS - 1) / 2;
    MapPixelCoordInt off_min(sampling_overhang, sampling_overhang);
    MapPixelDeltaInt off_max(sampling_overhang + 1, sampling_overhang + 1);

    MapPixelCoordInt i_pos(pos);
    i_pos.ClampToRect(off_min, MapPixelCoordInt(size - off_max));
    return i_pos;
}

// Given the requested map position and resulting m_center_int, return a
// BezierCoord which corresponds exactly to the requested map position.
BezierCoord MapBezier::FindCreationPos(const MapPixelCoord &d_pos,
                                       const MapPixelCoordInt &i_pos) const
{
    const int sampling_overhang = (N_POINTS - 1) / 2;
    MapPixelDelta offset = d_pos - MapPixelCoord(i_pos) +
                           MapPixelDelta(sampling_overhang, sampling_overhang);
    return BezierCoord(0.5 * offset.x, 0.5 * offset.y);
}

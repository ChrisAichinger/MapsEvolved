#define _USE_MATH_DEFINES
#include <memory>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

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


MapBezierPositioner::MapBezierPositioner(const MapPixelCoordInt &pos,
                                         const MapPixelDeltaInt &size)
    : m_center(pos), m_bezier_coord(0.5, 0.5),
      m_is_valid(!(m_center.x <= 0 || m_center.x >= size.x - 1 ||
                 m_center.y <= 0 || m_center.y >= size.y - 1 ||
                 m_bezier_coord.x < 0 || m_bezier_coord.x > 1 ||
                 m_bezier_coord.y < 0 || m_bezier_coord.y > 1))
{}

MapBezierPositioner::MapBezierPositioner(const MapPixelCoord &pos,
                                         const MapPixelDeltaInt &size)
    : m_center(FindCenter(pos, size)),
      m_bezier_coord(FindCreationPos(pos, m_center)),
      m_is_valid(!(m_center.x <= 0 || m_center.x >= size.x - 1 ||
                 m_center.y <= 0 || m_center.y >= size.y - 1 ||
                 m_bezier_coord.x < 0 || m_bezier_coord.x > 1 ||
                 m_bezier_coord.y < 0 || m_bezier_coord.y > 1))
{}

// Turn floating point map coordinates into an integer center for Bezier
MapPixelCoordInt
MapBezierPositioner::FindCenter(const MapPixelCoord &pos,
                                const MapPixelDeltaInt &size) const
{
    // Clamp to make sure we don't sample outside of the source
    // +1 in off_max because ClampToRect uses inclusive upper bound
    const int sampling_overhang = (Bezier::N_POINTS - 1) / 2;
    MapPixelCoordInt off_min(sampling_overhang, sampling_overhang);
    MapPixelDeltaInt off_max(sampling_overhang + 1, sampling_overhang + 1);

    MapPixelCoordInt i_pos(pos);
    i_pos.ClampToRect(off_min, MapPixelCoordInt(size - off_max));
    return i_pos;
}

BezierCoord
MapBezierPositioner::FindCreationPos(const MapPixelCoord &d_pos,
                                     const MapPixelCoordInt &i_pos) const
{
    const int sampling_overhang = (Bezier::N_POINTS - 1) / 2;
    MapPixelDelta offset = d_pos - MapPixelCoord(i_pos) +
                           MapPixelDelta(sampling_overhang, sampling_overhang);
    return BezierCoord(0.5 * offset.x, 0.5 * offset.y);
}


static inline double FastBezierCalc(const unsigned int *src,
                                    const MapPixelCoordInt &pos,
                                    const MapPixelDeltaInt &size,
                                    double x[3], double y[3])
{
    double X0 = 2.0 * x[0] - x[1];
    double X2 = 2.0 * x[2] - x[1];
    double Y0 = 2.0 * y[0] - y[1];
    double Y2 = 2.0 * y[2] - y[1];

// Sample a 3x3 area around pos.x/pos.y, thus the (-1)
#define SRC(xx,yy) ((int)src[(xx + pos.x - 1) + size.x * (yy + pos.y - 1)])
    return Y0 / 4 * (SRC(0, 0) * X0 + SRC(2, 0) * X2 + SRC(1, 0) * x[1] * 4) +
           Y2 / 4 * (SRC(0, 2) * X0 + SRC(2, 2) * X2 + SRC(1, 2) * x[1] * 4) +
           y[1] *   (SRC(0, 1) * X0 + SRC(2, 1) * X2 + SRC(1, 1) * 4 * x[1]);
#undef SRC
}

bool Gradient3x3(const unsigned int *src,
                 const MapPixelDeltaInt &src_size,
                 const MapPixelCoordInt &center,
                 const BezierCoord &bezier_pos,
                 MapBezierGradient *gradient)
{
    if (center.x <= 0 || center.x >= src_size.x - 1 ||
        center.y <= 0 || center.y >= src_size.y - 1 ||
        bezier_pos.x < 0 || bezier_pos.x > 1 ||
        bezier_pos.y < 0 || bezier_pos.y > 1)
    {
        return false;
    }

    double bx[Bezier::N_POINTS], by[Bezier::N_POINTS];
    double bx_deriv[Bezier::N_POINTS], by_deriv[Bezier::N_POINTS];
    Bezier::Bernstein_vec(Bezier::N_POINTS - 1, bezier_pos.x, bx);
    Bezier::Bernstein_vec(Bezier::N_POINTS - 1, bezier_pos.y, by);
    Bezier::Bernstein_deriv_vec(Bezier::N_POINTS - 1, bezier_pos.x, bx_deriv);
    Bezier::Bernstein_deriv_vec(Bezier::N_POINTS - 1, bezier_pos.y, by_deriv);

    gradient->x = FastBezierCalc(src, center, src_size, bx_deriv, by);
    gradient->y = FastBezierCalc(src, center, src_size, bx, by_deriv);
    return true;
}

bool Gradient3x3(const RasterMap &map,
                 const MapPixelCoordInt &center,
                 const BezierCoord &bezier_pos,
                 MapBezierGradient *gradient)
{
    MapPixelDeltaInt sampling_overhang((Bezier::N_POINTS - 1) / 2,
                                       (Bezier::N_POINTS - 1) / 2);
    MapPixelDeltaInt bezier_size(Bezier::N_POINTS, Bezier::N_POINTS);

    auto orig_data = map.GetRegion(center - sampling_overhang, bezier_size);
    return Gradient3x3(orig_data.GetRawData(), bezier_size,
                       MapPixelCoordInt(sampling_overhang), bezier_pos,
                       gradient);
}

bool Value3x3(const unsigned int *src,
                const MapPixelDeltaInt &src_size,
                const MapPixelCoordInt &center,
                const BezierCoord &bezier_pos,
                double *value)
{
    if (center.x <= 0 || center.x >= src_size.x - 1 ||
        center.y <= 0 || center.y >= src_size.y - 1 ||
        bezier_pos.x < 0 || bezier_pos.x > 1 ||
        bezier_pos.y < 0 || bezier_pos.y > 1)
    {
        return false;
    }

    double bx[Bezier::N_POINTS], by[Bezier::N_POINTS];
    Bezier::Bernstein_vec(Bezier::N_POINTS - 1, bezier_pos.x, bx);
    Bezier::Bernstein_vec(Bezier::N_POINTS - 1, bezier_pos.y, by);

    *value = FastBezierCalc(src, center, src_size, bx, by);
    return true;
}

bool Value3x3(const RasterMap &map,
                const MapPixelCoordInt &center,
                const BezierCoord &bezier_pos, double *value)
{
    MapPixelDeltaInt sampling_overhang((Bezier::N_POINTS - 1) / 2,
                                       (Bezier::N_POINTS - 1) / 2);
    MapPixelDeltaInt bezier_size(Bezier::N_POINTS, Bezier::N_POINTS);

    auto orig_data = map.GetRegion(center - sampling_overhang, bezier_size);
    return Value3x3(orig_data.GetRawData(), bezier_size,
                    MapPixelCoordInt(sampling_overhang), bezier_pos, value);
}


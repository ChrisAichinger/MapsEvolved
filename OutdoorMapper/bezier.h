#ifndef ODM__BEZIER_H
#define ODM__BEZIER_H

#include "coordinates.h"

class FromControlPoints {
    public:
        explicit FromControlPoints(const double *mat) : m_mat(mat) {};
        const double *Get() const { return m_mat; };
    private:
        const double *m_mat;
};

class FitData {
    public:
        explicit FitData(const double *mat) : m_mat(mat) {};
        const double *Get() const { return m_mat; };
    private:
        const double *m_mat;
};

class Bezier {
    public:
        static const unsigned int N_POINTS = 3;
        explicit Bezier(const FromControlPoints &points);
        explicit Bezier(const FitData &points);
        virtual ~Bezier() {};

        double GetValue(double x, double y);
        void GetGradient(double *x, double *y);

        static double Bernstein(unsigned int degree, unsigned int v, double x);
        static void Bernstein_vec(unsigned int degree, double x, double *out);
        static void Bernstein_deriv_vec(unsigned int degree, double x,
                                        double *out);
    protected:
        Bezier() {};
        void DoFitData();

        double m_points[N_POINTS * N_POINTS];
};


class MapBezierPositioner {
    public:
        explicit MapBezierPositioner(const MapPixelCoordInt &pos);
        explicit MapBezierPositioner(const MapPixelCoord &pos,
                                     const MapPixelDeltaInt &size);

        const MapPixelCoordInt &GetBezierCenter() const { return m_center; }
        const BezierCoord &GetBasePoint() const { return m_bezier_coord; }
    private:
        MapPixelCoordInt FindCenter(const MapPixelCoord &pos,
                                    const MapPixelDeltaInt &size) const;
        BezierCoord FindCreationPos(const MapPixelCoord &d_pos,
                                    const MapPixelCoordInt &i_pos) const;

        MapPixelCoordInt m_center;
        BezierCoord m_bezier_coord;
};


template <typename T>
static inline MapBezierGradient
Fast3x3CenterGradient(const T *src,
                      const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size)
{
    // sample at a cross pattern around pos:
    //         src10
    // src01    pos    src21
    //         src12
    double src01 = static_cast<double>(src[pos.x - 1 + size.x * pos.y]);
    double src21 = static_cast<double>(src[pos.x + 1 + size.x * pos.y]);
    double src10 = static_cast<double>(src[pos.x     + size.x * (pos.y - 1)]);
    double src12 = static_cast<double>(src[pos.x     + size.x * (pos.y + 1)]);

    // Bezier gradient at (0.5, 0.5) simplifies to this
    return MapBezierGradient(src21 - src01, src12 - src10);
}

MapBezierGradient Gradient3x3(const unsigned int *src,
                              const MapPixelDeltaInt &src_size,
                              const MapPixelCoordInt &center,
                              const BezierCoord &bezier_pos);

MapBezierGradient Gradient3x3(const class RasterMap &map,
                              const MapPixelCoordInt &center,
                              const BezierCoord &bezier_pos);

double Value3x3(const unsigned int *src,
                const MapPixelDeltaInt &src_size,
                const MapPixelCoordInt &center,
                const BezierCoord &bezier_pos);

double Value3x3(const class RasterMap &map,
                const MapPixelCoordInt &center,
                const BezierCoord &bezier_pos);
#endif

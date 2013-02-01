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
    protected:
        Bezier() {};
        void DoFitData();

        double m_points[N_POINTS * N_POINTS];
    private:
        static double Bernstein(unsigned int degree, unsigned int v, double x);
        static void Bernstein_vec(unsigned int degree, double x, double *out);
        static void Bernstein_deriv_vec(unsigned int degree, double x,
                                        double *out);
};

class MapBezier : public Bezier {
    public:
        MapBezier(const class RasterMap &map, const MapPixelCoordInt &pos);
        MapBezier(const class RasterMap &map, const MapPixelCoord &pos);
        MapBezier(const unsigned int *src,
                  const MapPixelCoordInt &pos, const MapPixelDeltaInt &size);
        MapBezier(const unsigned int *src,
                  const MapPixelCoord &pos, const MapPixelDeltaInt &size);

        const MapPixelCoordInt &GetBezierCenter() const {
            return m_center_int;
        }
        const BezierCoord &GetCreationPos() const {
            return m_creation_pos;
        }

        using Bezier::GetValue;
        double GetValue(const BezierCoord &pos) {
            return GetValue(pos.x, pos.y);
        }
        using Bezier::GetGradient;
        MapBezierGradient GetGradient(const BezierCoord &pos) {
            MapBezierGradient grad(pos.x, pos.y);
            GetGradient(&grad.x, &grad.y);
            return grad;
        }
    private:
        MapPixelCoordInt m_center_int;
        BezierCoord m_creation_pos;

        void InitPoints(const unsigned int *src,
                        const MapPixelCoordInt &pos,
                        const MapPixelDeltaInt &size,
                        bool invert_y);
        void InitPoints(const class RasterMap &map,
                        const MapPixelCoordInt &pos);
        MapPixelCoordInt FindCenter(const MapPixelCoord &pos,
                                 const MapPixelDeltaInt &size) const;
        BezierCoord FindCreationPos(const MapPixelCoord &d_pos,
                                    const MapPixelCoordInt &i_pos) const;
};

#endif

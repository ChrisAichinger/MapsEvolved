#ifndef ODM__BEZIER_H
#define ODM__BEZIER_H

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
        MapBezier(const class RasterMap &map,
                  double *x, double *y);
        MapBezier(const unsigned int *src,
                  unsigned int width, unsigned int height,
                  double *x, double *y);
        MapBezier(const class RasterMap &map,
                  unsigned int x, unsigned int y);
        MapBezier(const unsigned int *src,
                  unsigned int width, unsigned int height,
                  unsigned int x, unsigned int y);
        unsigned int GetCenterX() const { return m_center_x; }
        unsigned int GetCenterY() const { return m_center_y; }
    private:
        unsigned int m_center_x, m_center_y;

        void InitPoints(const unsigned int *src,
                        unsigned int width, unsigned int height,
                        unsigned int x, unsigned int y,
                        bool invert_y=false);
        void PointFromDouble(double *x, double *y,
                             unsigned int *x_int, unsigned int *y_int,
                             unsigned int width, unsigned int height);
};

#endif

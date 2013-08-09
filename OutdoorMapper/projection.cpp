#include <stdexcept>
#include <cmath>

#include <GeographicLib\Geodesic.hpp>

#include "projection.h"
#include "proj_api.h"
#include "rastermap.h"

class ProjWrap {
    public:
        explicit ProjWrap(projPJ proj);
        ~ProjWrap();
        projPJ Get() { return m_proj; };
        bool IsValid() const { return !!m_proj; };
    private:
        projPJ m_proj;
};

ProjWrap::ProjWrap(projPJ proj)
    : m_proj(proj)
    {};

ProjWrap::~ProjWrap() {
    if (m_proj) {
        pj_free(m_proj);
        m_proj = NULL;
    }
}

Projection::Projection(const std::string &proj_str)
    : m_proj(new ProjWrap(pj_init_plus(proj_str.c_str()))),
      m_proj_str(proj_str)
{
    m_is_valid = m_proj->IsValid();
}

bool Projection::PCSToLatLong(double &x, double &y) const {
    if (!m_is_valid)
        return false;

    projXY pcs = {x, y};
    projLP latlong = pj_inv(pcs, m_proj->Get());
    if (latlong.u == HUGE_VAL || latlong.v == HUGE_VAL) {
        return false;
    }
    x = latlong.u * RAD_to_DEG;
    y = latlong.v * RAD_to_DEG;
    return true;
}

bool Projection::LatLongToPCS(double &x, double &y) const {
    if (!m_is_valid)
        return false;

    projLP latlong = {x * DEG_to_RAD, y * DEG_to_RAD};
    projXY pcs = pj_fwd(latlong, m_proj->Get());
    if (pcs.u == HUGE_VAL || pcs.v == HUGE_VAL) {
        return false;
    }
    x = pcs.u;
    y = pcs.v;
    return true;
}

double flattening_from_excentricity_squared(double e2) {
    // cf. http://www.arsitech.com/mapping/geodetic_datum/
    return 1 - sqrt(1 - e2);
}

bool Projection::CalcDistance(double lat1, double long1,
                                double lat2, double long2,
                                double *distance) const
{
    if (!m_is_valid)
        return false;

    double a, e2; // major axis and excentricity squared
    pj_get_spheroid_defn(m_proj->Get(), &a, &e2);

    double s12;
    double f = flattening_from_excentricity_squared(e2);
    GeographicLib::Geodesic geod(a, f);
    geod.Inverse(lat1, long1, lat2, long2, s12);
    *distance = s12;
    return true;
}

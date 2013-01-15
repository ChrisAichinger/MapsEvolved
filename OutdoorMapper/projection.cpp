#include <stdexcept>
#include <cmath>

#include <GeographicLib\Geodesic.hpp>

#include "projection.h"
#include "proj_api.h"
#include "rastermap.h"

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
    if (!m_proj) {
        throw std::runtime_error("Failed to initialize projection.");
    }
}

void Projection::PCSToLatLong(double &x, double &y) const {
    projXY pcs = {x, y};
    projLP latlong = pj_inv(pcs, m_proj->Get());
    if (latlong.u == HUGE_VAL || latlong.v == HUGE_VAL) {
        throw std::runtime_error("Can't convert projected CS to Lat/Long.");
    }
    x = latlong.u * RAD_TO_DEG;
    y = latlong.v * RAD_TO_DEG;
}

void Projection::LatLongToPCS(double &x, double &y) const {
    projLP latlong = {x * DEG_TO_RAD, y * DEG_TO_RAD};
    projXY pcs = pj_fwd(latlong, m_proj->Get());
    if (pcs.u == HUGE_VAL || pcs.v == HUGE_VAL) {
        throw std::runtime_error("Can't convert projected CS to Lat/Long.");
    }
    x = pcs.u;
    y = pcs.v;
}

double flattening_from_excentricity_squared(double e2) {
    // cf. http://www.arsitech.com/mapping/geodetic_datum/
    return 1 - sqrt(1 - e2);
}

double Projection::CalcDistance(double long1, double lat1,
                                double long2, double lat2) const
{
    double a, e2; // major axis and excentricity squared
    pj_get_spheroid_defn(m_proj->Get(), &a, &e2);

    double s12;
    double f = flattening_from_excentricity_squared(e2);
    GeographicLib::Geodesic geod(a, f);
    geod.Inverse(lat1, long1, lat2, long2, s12);
    return s12;
}

#include <stdexcept>

#include "projection.h"
#include "proj_api.h"
#include "rastermap.h"

Projection::Projection(const std::string &proj_str) 
    : m_proj(pj_init_plus(proj_str.c_str())),
      m_proj_str(proj_str)
{
    if (!m_proj) {
        throw std::runtime_error("Failed to initialize projection.");
    }
}

Projection::~Projection() {
    if (m_proj) {
        pj_free(m_proj);
        m_proj = NULL;
    }
}

void Projection::PCSToLatLong(double &x, double &y) const {
    projXY pcs = {x, y};
    projLP latlong = pj_inv(pcs, m_proj);
    if (latlong.u == HUGE_VAL || latlong.v == HUGE_VAL) {
        throw std::runtime_error("Can't convert projected CS to Lat/Long.");
    }
    x = latlong.u * RAD_TO_DEG;
    y = latlong.v * RAD_TO_DEG;
}

void Projection::LatLongToPCS(double &x, double &y) const {
    projLP latlong = {x * DEG_TO_RAD, y * DEG_TO_RAD};
    projXY pcs = pj_fwd(latlong, m_proj);
    if (pcs.u == HUGE_VAL || pcs.v == HUGE_VAL) {
        throw std::runtime_error("Can't convert projected CS to Lat/Long.");
    }
    x = pcs.u;
    y = pcs.v;
}

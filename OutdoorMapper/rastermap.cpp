#include <string>
#include <memory>
#include <tuple>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>
#include <assert.h>
#include <algorithm>
#include <cctype>

#include <GeographicLib\Geodesic.hpp>

#include "util.h"
#include "rastermap.h"
#include "map_geotiff.h"
#include "map_dhm_advanced.h"
#include "bezier.h"
#include "projection.h"

RasterMap::~RasterMap() {};

RasterMapCollection::RasterMapCollection()
    : m_maps()
{ }

void RasterMapCollection::AddMap(std::shared_ptr<class RasterMap> map) {
    m_maps.push_back(map);
}

void LoadMap(RasterMapCollection &maps, const std::wstring &fname) {
    std::wstring fname_lower(fname);
    std::transform(fname_lower.begin(), fname_lower.end(),
                   fname_lower.begin(), ::towlower);

    std::shared_ptr<class RasterMap> map;
    if (ends_with(fname_lower, L".tif") || ends_with(fname_lower, L".tiff")) {
        // Geotiff file
        map.reset(new TiffMap(fname.c_str()));
        maps.AddMap(map);
    } else {
        assert(false);  // Not implemented
    }

    if (map->GetType() == RasterMap::TYPE_DHM) {
        map.reset(new GradientMap(map));
        maps.AddMap(map);
    }
}

HeightFinder::HeightFinder(const class RasterMapCollection &maps)
    : m_maps(maps), m_active_dhm(0)
{ }

bool HeightFinder::CalcTerrain(double lat, double lon, double *height,
                               double *slope_face, double *steepness_deg)
{
    TerrainInfo ti;
    bool res = CalcTerrain(lat, lon, &ti);
    if (!res)
        return res;

    if (height)
        *height = ti.height;
    if (slope_face)
        *slope_face = ti.slope_face;
    if (steepness_deg)
        *steepness_deg = ti.steepness_deg;

    return res;
}

bool HeightFinder::CalcTerrain(double lat, double lon, TerrainInfo *result) {
    assert(result);

    if (!LatLongWithinActiveDHM(lat, lon)) {
        m_active_dhm = FindBestMap(lat, lon, RasterMap::TYPE_DHM);
        if (!m_active_dhm)
            return false;
    }
    double x = lat;
    double y = lon;
    m_active_dhm->LatLongToPixel(&x, &y);
    MapBezier bezier(*m_active_dhm, &x, &y);

    unsigned int cx = bezier.GetCenterX();
    unsigned int cy = bezier.GetCenterY();
    unsigned int bezier_pixels = bezier.N_POINTS - 1;
    double bezier_meters = bezier_pixels * MetersPerPixel(m_active_dhm, cx, cy);

    double gradx = x, grady = y;
    bezier.GetGradient(&gradx, &grady);
    gradx /= bezier_meters;
    grady /= bezier_meters;
    double grad_direction = atan2(grady, gradx);
    double grad_abs = sqrt(gradx*gradx + grady*grady);
    double grad_steepness = atan(grad_abs);

    result->height = bezier.GetValue(x, y);
    result->slope_face = normalize_direction(270 + grad_direction*RAD_to_DEG);
    result->steepness_deg = grad_steepness * RAD_to_DEG;
    return true;
}

bool HeightFinder::LatLongWithinActiveDHM(double x, double y) const {
    if (!m_active_dhm)
        return false;
    m_active_dhm->LatLongToPixel(&x, &y);
    if (x < 0 ||
        y < 0 ||
        x >= m_active_dhm->GetWidth() ||
        y >= m_active_dhm->GetHeight()) {

        return false;
    }
    return true;
}

const class RasterMap *
HeightFinder::FindBestMap(
        double latitude, double longitude,
        RasterMap::RasterMapType type) const
{
    for (unsigned int i=0; i < m_maps.Size(); i++) {
        const RasterMap &map = m_maps.Get(i);
        if (map.GetType() == type)
            return &map;
    }
    return NULL;
}

double GetMapDistance(const RasterMap &map, double Cx, double Cy, double dx, double dy) {
    double Ax = Cx - dx;
    double Ay = Cy - dy;
    double Bx = Cx + dx;
    double By = Cy + dy;
    map.PixelToLatLong(&Ax, &Ay);
    map.PixelToLatLong(&Bx, &By);
    Projection proj = map.GetProj();
    return proj.CalcDistance(Ay, Ax, By, Bx);
}

double MetersPerPixel(const RasterMap *map, double x_px, double y_px) {
    double mppx = 0.5 * GetMapDistance(*map, x_px, y_px, 1, 0);
    double mppy = 0.5 * GetMapDistance(*map, x_px, y_px, 0, 1);
    return 0.5 * (mppx + mppy);
}

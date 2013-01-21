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

double HeightFinder::GetHeight(double latitude, double longitude) {
    double height = 0.0;
    // return 0.0 if CalcTerrain() fails
    CalcTerrain(latitude, longitude, &height, NULL, NULL, NULL);
    return height;
}

bool HeightFinder::CalcTerrain(double lat, double lon, double *height,
                               double *slope_face, double *steepness_deg,
                               double *meter_per_pixel)
{
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
    unsigned int delta = (bezier.N_POINTS - 1) / 2;

    double Lx = cx - delta, Ly = cy;
    double Rx = cx + delta, Ry = cy;
    double Tx = cx, Ty = cy - delta;
    double Bx = cx, By = cy + delta;
    m_active_dhm->PixelToLatLong(&Lx, &Ly);
    m_active_dhm->PixelToLatLong(&Rx, &Ry);
    m_active_dhm->PixelToLatLong(&Tx, &Ty);
    m_active_dhm->PixelToLatLong(&Bx, &By);

    Projection proj = m_active_dhm->GetProj();
    double gradx = x, grady = y;
    bezier.GetGradient(&gradx, &grady);
    double distance_x = proj.CalcDistance(Ly, Lx, Ry, Rx);
    double distance_y = proj.CalcDistance(Ty, Tx, By, Bx);
    gradx /= distance_x;
    grady /= distance_y;
    double grad_direction = atan2(grady, gradx);
    double grad_abs = sqrt(gradx*gradx + grady*grady);
    double grad_steepness = atan(grad_abs);

    if (height)
        *height = bezier.GetValue(x, y);
    if (slope_face)
        *slope_face = normalize_direction(270 + grad_direction * RAD_to_DEG);
    if (steepness_deg)
        *steepness_deg = grad_steepness * RAD_to_DEG;
    if (meter_per_pixel) {
        // distance_x and _y are between pixel (n - delta) and (n + delta)
        *meter_per_pixel = 0.5 * (distance_x + distance_y) / (2*delta);
    }
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

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

#include "util.h"
#include "rastermap.h"
#include "map_geotiff.h"
#include "map_dhm_advanced.h"
#include "bezier.h"

RasterMap::~RasterMap() {};

RasterMapCollection::RasterMapCollection()
    : m_main_idx(0), m_maps()
{ }

void RasterMapCollection::AddMap(std::shared_ptr<class RasterMap> map) {
    m_maps.push_back(map);
    m_main_idx = m_maps.size() - 1;
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
    if (!LatLongWithinActiveDHM(latitude, longitude)) {
        m_active_dhm = FindBestMap(latitude, longitude, RasterMap::TYPE_DHM);
        if (!m_active_dhm)
            return 0;
    }
    double x = latitude;
    double y = longitude;
    m_active_dhm->LatLongToPixel(&x, &y);
    MapBezier bezier(*m_active_dhm, &x, &y);
    return bezier.GetValue(x, y);
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

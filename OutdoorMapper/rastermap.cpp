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


//#include <Windows.h>

#include "util.h"
#include "rastermap.h"
#include "map_geotiff.h"

RasterMap::~RasterMap() {};


RasterMapCollection::RasterMapCollection()
    : m_main_idx(0), m_maps()
{ }

void RasterMapCollection::AddMap(std::shared_ptr<class RasterMap> map) {
    m_maps.push_back(map);
    m_main_idx = m_maps.size() - 1;
}

std::shared_ptr<class RasterMap> LoadMap(const std::wstring &fname) {
    std::wstring fname_lower(fname);
    std::transform(fname_lower.begin(), fname_lower.end(),
                   fname_lower.begin(), ::towlower);

    if (ends_with(fname_lower, L".tif") || ends_with(fname_lower, L".tiff")) {
        // Geotiff file
        return std::shared_ptr<class RasterMap>(new TiffMap(fname.c_str()));
    }

    assert(false);  // Not implemented
    return std::shared_ptr<class RasterMap>();
}

HeightFinder::HeightFinder(const class RasterMapCollection &maps)
    : m_maps(maps), m_active_dhm(0)
{ }

double HeightFinder::GetHeight(double latitude, double longitude) {
    if (!LatLongWithinActiveDHM(latitude, longitude)) {
        LoadBestDHM(latitude, longitude);
        if (!m_active_dhm)
            return 0;
    }
    double x = latitude;
    double y = longitude;
    m_active_dhm->LatLongToPixel(&x, &y);

    int ix = round_to_neg_inf(x);
    int iy = round_to_neg_inf(y);
    std::shared_ptr<unsigned int> pixels(
            m_active_dhm->GetRegion(ix, iy, 2, 2),
            ArrayDeleter<unsigned int>());

    double row1_avg = lerp(x - ix, (int)pixels.get()[0], (int)pixels.get()[1]);
    double row2_avg = lerp(x - ix, (int)pixels.get()[2], (int)pixels.get()[3]);
    return lerp(y - iy, row1_avg, row2_avg);
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

void HeightFinder::LoadBestDHM(double latitude, double longitude) {
    m_active_dhm = NULL;
    for (unsigned int i=0; i < m_maps.Size(); i++) {
        const RasterMap &map = m_maps.Get(i);
        if (map.GetType() != RasterMap::TYPE_DHM)
            continue;

        m_active_dhm = &map;
    }
}

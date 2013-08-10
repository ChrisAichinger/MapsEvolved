#include "rastermap.h"

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
#include <limits>

#include <GeographicLib\Geodesic.hpp>

#include "util.h"
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

void RasterMapCollection::DeleteMap(unsigned int index) {
    m_maps.erase(m_maps.begin() + index);
}

bool RasterMapCollection::StoreTo(PersistentStore *store) const {
    if (!store->IsOpen())
        store->OpenWrite();

    std::vector<std::wstring> filenames;
    filenames.reserve(m_maps.size());
    for (auto it = m_maps.cbegin(); it != m_maps.cend(); ++it) {
        std::shared_ptr<RasterMap> map = *it;
        if (map->GetType() == RasterMap::TYPE_GRADIENT ||
            map->GetType() == RasterMap::TYPE_STEEPNESS)
        {
            continue;
        }
        filenames.push_back(map->GetFname());
    }
    return store->SetStringList(L"maps", filenames);
}

bool RasterMapCollection::RetrieveFrom(PersistentStore *store) {
    if (!store->IsOpen())
        store->OpenRead();

    std::vector<std::wstring> filenames;
    if (!store->GetStringList(L"maps", &filenames)) {
        return false;
    }

    for (auto it = filenames.cbegin(); it != filenames.cend(); ++it) {
        LoadMap(*this, *it);
    }
    return true;
}


class RasterMapError : public RasterMap {
    public:
        explicit RasterMapError(const wchar_t *fname, const std::wstring &desc)
            : m_fname(fname), m_desc(desc)
            {}
        virtual RasterMap::RasterMapType GetType() const { return TYPE_ERROR; }
        virtual unsigned int GetWidth() const { return 0; }
        virtual unsigned int GetHeight() const { return 0; }
        virtual MapPixelDeltaInt GetSize() const
            { return MapPixelDeltaInt(0,0); }
        virtual std::shared_ptr<unsigned int>
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &sz) const
        {
            return std::shared_ptr<unsigned int>(new unsigned int[sz.x*sz.y](),
                                                 ArrayDeleter<unsigned int>());
        }
        virtual bool PixelToPCS(double *x, double *y) const { return false; }
        virtual bool PCSToPixel(double *x, double *y) const { return false; }
        virtual Projection GetProj() const {
            assert(false);
            return Projection("");
        }

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
            return false;
        }
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
            return false;
        }
        virtual const std::wstring &GetFname() const { return m_fname; }
        virtual const std::wstring &GetDescription() const {
            return m_fname;
        }

    private:
        const std::wstring m_fname;
        const std::wstring m_desc;
};


void LoadMap(RasterMapCollection &maps, const std::wstring &fname) {
    std::wstring fname_lower(fname);
    std::transform(fname_lower.begin(), fname_lower.end(),
                   fname_lower.begin(), ::towlower);

    std::shared_ptr<class RasterMap> map;
    try {
        if (ends_with(fname_lower, L".tif") ||
            ends_with(fname_lower, L".tiff"))
        {
            // Geotiff file
            map.reset(new TiffMap(fname.c_str()));
            maps.AddMap(map);
        } else {
            assert(false);  // Not implemented
        }

        if (map->GetType() == RasterMap::TYPE_DHM) {
            std::shared_ptr<class RasterMap> deriv_map;
            deriv_map.reset(new GradientMap(map));
            maps.AddMap(deriv_map);
            deriv_map.reset(new SteepnessMap(map));
            maps.AddMap(deriv_map);
        }
    } catch (const std::runtime_error &) {
        map.reset(new RasterMapError(fname.c_str(), L"Exception"));
        maps.AddMap(map);
    }
}

HeightFinder::HeightFinder(const class RasterMapCollection &maps)
    : m_maps(maps), m_active_dhm(0)
{ }

bool HeightFinder::CalcTerrain(const LatLon &pos, TerrainInfo *result) {
    assert(result);

    if (!LatLongWithinActiveDHM(pos)) {
        m_active_dhm = FindBestMap(pos, RasterMap::TYPE_DHM);
        if (!m_active_dhm)
            return false;
    }
    MapPixelCoord map_pos;
    if (!m_active_dhm->LatLonToPixel(pos, &map_pos)) {
        return false;
    }
    MapBezierPositioner bezier_pos(map_pos, m_active_dhm->GetSize());
    if (!bezier_pos.IsValid()) {
        return false;
    }
    double mpp;
    if (!MetersPerPixel(*m_active_dhm, bezier_pos.GetBezierCenter(), &mpp)) {
        return false;
    }
    unsigned int bezier_pixels = Bezier::N_POINTS - 1;
    double bezier_meters = bezier_pixels * mpp;

    MapBezierGradient grad;
    if (!Gradient3x3(*m_active_dhm, bezier_pos.GetBezierCenter(),
                     bezier_pos.GetBasePoint(), &grad))
    {
        return false;
    }

    grad /= bezier_meters;
    // -grad.y corrects for the coordinate system of the image pixels not being
    // the same as the map coordinates (y axis inverted)
    double grad_direction = atan2(-grad.y, grad.x);
    double grad_steepness = atan(grad.Abs());

    if (!Value3x3(*m_active_dhm, bezier_pos.GetBezierCenter(),
                  bezier_pos.GetBasePoint(), &result->height_m))
    {
        return false;
    }
    result->steepness_deg = grad_steepness * RAD_to_DEG;
    result->slope_face_deg = normalize_direction(270 +
                                                 grad_direction * RAD_to_DEG);
    return true;
}

bool HeightFinder::LatLongWithinActiveDHM(const LatLon &pos) const {
    if (!m_active_dhm)
        return false;
    MapPixelCoord map_pos;
    if (!m_active_dhm->LatLonToPixel(pos, &map_pos))
        return false;
    return map_pos.IsInRect(MapPixelCoord(0, 0),
                            MapPixelDelta(m_active_dhm->GetSize()));
}

const class RasterMap *
HeightFinder::FindBestMap(const LatLon &pos,
                          RasterMap::RasterMapType type) const
{
    for (unsigned int i=0; i < m_maps.Size(); i++) {
        const RasterMap &map = m_maps.Get(i);
        if (map.GetType() == type)
            return &map;
    }
    return NULL;
}

bool GetMapDistance(const RasterMap &map, const MapPixelCoord &pos,
                    double dx, double dy, double *distance)
{
    MapPixelCoord A(pos.x - dx, pos.y - dy);
    MapPixelCoord B(pos.x + dx, pos.y + dy);
    LatLon lA, lB;
    if (!map.PixelToLatLon(A, &lA) || !map.PixelToLatLon(B, &lB)) {
        return false;
    }
    Projection proj = map.GetProj();
    return proj.CalcDistance(lA.lat, lA.lon, lB.lat, lB.lon, distance);
}

bool MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos,
                    double *mpp)
{
    double mppx, mppy;
    if (!GetMapDistance(map, pos, 1, 0, &mppx) ||
        !GetMapDistance(map, pos, 0, 1, &mppy))
    {
        return false;
    }

    // 0.5 -> average
    // 0.5 -> 2 pixels difference calculated within GetMapDistance
    *mpp = 0.5 * 0.5 * (mppx + mppy);
    return true;
}

bool MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos,
                    double *mpp)
{
    return MetersPerPixel(map, MapPixelCoord(pos), mpp);
}

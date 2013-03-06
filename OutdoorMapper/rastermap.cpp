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
        if (map->GetType() == RasterMap::TYPE_GRADIENT) {
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
        virtual void PixelToPCS(double *x, double *y) const {}
        virtual void PCSToPixel(double *x, double *y) const {}
        virtual Projection GetProj() const {
            assert(false);
            return Projection("");
        }

        virtual LatLon PixelToLatLon(const MapPixelCoord &pos) const {
            return LatLon();
        }
        virtual MapPixelCoord LatLonToPixel(const LatLon &pos) const {
            return MapPixelCoord();
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
            map.reset(new GradientMap(map));
            maps.AddMap(map);
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
    MapPixelCoord map_pos = m_active_dhm->LatLonToPixel(pos);
    MapBezierPositioner bezier_position(map_pos, m_active_dhm->GetSize());

    unsigned int bezier_pixels = Bezier::N_POINTS - 1;
    double bezier_meters = bezier_pixels *
                     MetersPerPixel(*m_active_dhm, bezier_position.GetBezierCenter());

    MapBezierGradient grad = Gradient3x3(*m_active_dhm,
                                         bezier_position.GetBezierCenter(),
                                         bezier_position.GetBasePoint());
    grad /= bezier_meters;
    // -grad.y corrects for the coordinate system of the image pixels not being
    // the same as the map coordinates (y axis inverted)
    double grad_direction = atan2(-grad.y, grad.x);
    double grad_steepness = atan(grad.Abs());

    result->height_m = Value3x3(*m_active_dhm,
                                bezier_position.GetBezierCenter(),
                                bezier_position.GetBasePoint());
    result->steepness_deg = grad_steepness * RAD_to_DEG;
    result->slope_face_deg = normalize_direction(270 +
                                                 grad_direction * RAD_to_DEG);
    return true;
}

bool HeightFinder::LatLongWithinActiveDHM(const LatLon &pos) const {
    if (!m_active_dhm)
        return false;
    MapPixelCoord map_pos = m_active_dhm->LatLonToPixel(pos);
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

double GetMapDistance(const RasterMap &map, const MapPixelCoord &pos,
                      double dx, double dy)
{
    MapPixelCoord A(pos.x - dx, pos.y - dy);
    MapPixelCoord B(pos.x + dx, pos.y + dy);
    LatLon lA = map.PixelToLatLon(A);
    LatLon lB = map.PixelToLatLon(B);
    Projection proj = map.GetProj();
    return proj.CalcDistance(lA.lat, lA.lon, lB.lat, lB.lon);
}

double MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos) {
    double mppx = 0.5 * GetMapDistance(map, pos, 1, 0);
    double mppy = 0.5 * GetMapDistance(map, pos, 0, 1);
    return 0.5 * (mppx + mppy);
}

double MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos) {
    return MetersPerPixel(map, MapPixelCoord(pos));
}

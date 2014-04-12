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
#include <cmath>
#include <stdio.h>

#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/LocalCartesian.hpp>

#include "util.h"
#include "map_geotiff.h"
#include "map_dhm_advanced.h"
#include "bezier.h"
#include "projection.h"
#include "drawing.h"

GeoPixels::~GeoPixels() {};
GeoDrawable::~GeoDrawable() {};
RasterMap::~RasterMap() {};

void GetAlternateRepresentation(const std::shared_ptr<RasterMap> &map,
                std::vector<std::shared_ptr<RasterMap> > &representations)
{
    representations.clear();
    if (map->GetType() == RasterMap::TYPE_DHM) {
        std::shared_ptr<class RasterMap> deriv_map(new GradientMap(map));
        representations.push_back(deriv_map);
        deriv_map.reset(new SteepnessMap(map));
        representations.push_back(deriv_map);
    }
}


RasterMapCollection::~RasterMapCollection() { }

DefaultRasterMapCollection::DefaultRasterMapCollection()
    : m_maps()
{ }

void DefaultRasterMapCollection::AddMap(const std::shared_ptr<class RasterMap> &map) {
    struct DefaultRasterMapCollection::MapAndReps insert;
    insert.map = map;
    if (map->GetType() == RasterMap::TYPE_DHM) {
        std::shared_ptr<class RasterMap> deriv_map(new GradientMap(map));
        insert.reps.push_back(deriv_map);
        deriv_map.reset(new SteepnessMap(map));
        insert.reps.push_back(deriv_map);
    }
    m_maps.push_back(insert);
}

void DefaultRasterMapCollection::DeleteMap(unsigned int index) {
    m_maps.erase(m_maps.begin() + index);
}

bool DefaultRasterMapCollection::StoreTo(
        const std::unique_ptr<PersistentStore> &store) const
{
    if (!store->IsOpen())
        store->OpenWrite();

    std::vector<std::wstring> filenames;
    filenames.reserve(m_maps.size());
    for (auto it = m_maps.cbegin(); it != m_maps.cend(); ++it) {
        filenames.push_back(it->map->GetFname());
    }
    return store->SetStringList(L"maps", filenames);
}

bool DefaultRasterMapCollection::RetrieveFrom(
        const std::unique_ptr<PersistentStore> &store)
{
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

bool DefaultRasterMapCollection::IsToplevelMap(
        const std::shared_ptr<RasterMap> &map) const
{
    for (auto it = m_maps.cbegin(); it != m_maps.cend(); ++it) {
        if (it->map == map)
            return true;
    }
    return false;
}

class RasterMapError : public RasterMap {
    public:
        explicit RasterMapError(const wchar_t *fname, const std::wstring &desc)
            : m_fname(fname), m_desc(desc)
            {}
        virtual GeoDrawable::DrawableType GetType() const {
            return TYPE_ERROR;
        }
        virtual unsigned int GetWidth() const { return 0; }
        virtual unsigned int GetHeight() const { return 0; }
        virtual MapPixelDeltaInt GetSize() const
            { return MapPixelDeltaInt(0,0); }
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &sz) const
        {
            return MapRegion(std::shared_ptr<unsigned int>(
                            new unsigned int[sz.x*sz.y](),
                            ArrayDeleter<unsigned int>()),
                     sz.x, sz.y);
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
        virtual ODMPixelFormat GetPixelFormat() const { return ODM_PIX_RGBX4; }

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
    if (!MetersPerPixel(m_active_dhm, bezier_pos.GetBezierCenter(), &mpp)) {
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

std::shared_ptr<class RasterMap>
HeightFinder::FindBestMap(const LatLon &pos,
                          GeoDrawable::DrawableType type) const
{
    for (unsigned int i=0; i < m_maps.Size(); i++) {
        auto map = m_maps.Get(i);
        if (map->GetType() == type)
            return map;
    }
    return NULL;
}

bool GetMapDistance(const std::shared_ptr<class GeoDrawable> &map,
                    const MapPixelCoord &pos,
                    double dx, double dy, double *distance)
{
    MapPixelCoord A(pos.x - dx, pos.y - dy);
    MapPixelCoord B(pos.x + dx, pos.y + dy);
    LatLon lA, lB;
    if (!map->PixelToLatLon(A, &lA) || !map->PixelToLatLon(B, &lB)) {
        return false;
    }
    Projection proj = map->GetProj();
    return proj.CalcDistance(lA.lat, lA.lon, lB.lat, lB.lon, distance);
}

bool MetersPerPixel(const std::shared_ptr<class GeoDrawable> &map,
                    const MapPixelCoord &pos,
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

bool MetersPerPixel(const std::shared_ptr<class GeoDrawable> &map,
                    const MapPixelCoordInt &pos,
                    double *mpp)
{
    return MetersPerPixel(map, MapPixelCoord(pos), mpp);
}

bool
GetMapDistance(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoord &pos,
               double dx, double dy, double *distance)
{
    return GetMapDistance(std::static_pointer_cast<GeoDrawable>(map),
                          pos, dx, dy, distance);
}
bool
MetersPerPixel(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoord &pos,
               double *mpp)
{
    return MetersPerPixel(std::static_pointer_cast<GeoDrawable>(map),
                          pos, mpp);
}
bool
MetersPerPixel(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoordInt &pos,
               double *mpp)
{
    return MetersPerPixel(std::static_pointer_cast<GeoDrawable>(map),
                          pos, mpp);
}


struct PanoramaListEntry {
    int y;
    double distance;

    PanoramaListEntry() : y(0), distance(0) {};
    PanoramaListEntry(int yy, double ddistance) : y(yy), distance(ddistance) {};
};

struct PanoramaList {
    PanoramaListEntry highpoint;
    std::vector<PanoramaListEntry> points;

    PanoramaList() : highpoint(std::numeric_limits<int>::max(), 0), points() {};
};

static bool CalcPanoramaOneTile(const std::shared_ptr<GeoDrawable> &map,
                                const GeographicLib::LocalCartesian &proj,
                                const MapPixelCoordInt &tile_coord,
                                const MapPixelDeltaInt &tile_size,
                                std::vector<PanoramaList> &highpoints,
                                unsigned int *dest,
                                const MapPixelDeltaInt &output_size)
{
    unsigned int scale = output_size.x / 360;
    auto region = map->GetRegion(tile_coord, tile_size);
    const unsigned int *data = region.GetRawData();
    for (unsigned int py = 0; py < region.GetHeight(); py++) {
        for (unsigned int px = 0; px < region.GetWidth(); px++) {
            short int ele = static_cast<short int>(region.GetPixel(px, py));
            MapPixelCoord pixel_coord(tile_coord.x + px, tile_coord.y + py);
            LatLon ll;
            if (!map->PixelToLatLon(pixel_coord, &ll)) {
                return false;
            }
            double x, y, z;
            proj.Forward(ll.lat, ll.lon, ele, x, y, z);
            double heading = atan2(x, y) * RAD_to_DEG;
            heading += (heading < 0) ? 360 : 0;

            double horiz_distance = sqrt(x*x + y*y);
            double elevation_angle = atan(z / horiz_distance) * RAD_to_DEG;
            // We modulo vs the output size, otherwise we can get
            // pixel_x == output_size.x (=> vector overflow) due to rounding.
            int pixel_x = round_to_int(scale * heading);
            if (pixel_x > output_size.x) {
                assert(false);
            }
            pixel_x %= output_size.x;
            int pixel_y = output_size.y / 2 + round_to_int(scale * elevation_angle);
            PanoramaList &cur = highpoints[pixel_x];
            if (pixel_y > cur.highpoint.y && horiz_distance >= cur.highpoint.distance) {
                continue;
            }

            if (pixel_y > cur.highpoint.y) {
                cur.highpoint.y = pixel_y;
                cur.highpoint.distance = horiz_distance;
            }
            cur.points.push_back(PanoramaListEntry(pixel_y, horiz_distance));
        }
    }
    return true;
}

MapRegion
CalcPanorama(const std::shared_ptr<GeoDrawable> &map, const LatLon &pos) {
    static const unsigned int TILE_SIZE = 512;
    if (!(map->GetType() == GeoDrawable::TYPE_DHM)) {
        return MapRegion();
    }
    MapPixelCoord mp_pos;
    if (!map->LatLonToPixel(pos, &mp_pos)) {
        return MapRegion();
    }
    MapPixelCoordInt mp_pos_int(static_cast<unsigned int>(mp_pos.x),
                                static_cast<unsigned int>(mp_pos.y));
    auto region = map->GetRegion(mp_pos_int, MapPixelDeltaInt(2, 2));
    double dx = mp_pos.x - mp_pos_int.x;
    double dy = mp_pos.y - mp_pos_int.y;
    double pos_elevation = lerp(dy,
            lerp(dx, region.GetPixel(0, 0), region.GetPixel(1, 0)),
            lerp(dx, region.GetPixel(0, 1), region.GetPixel(1, 1))) + 50;
    GeographicLib::LocalCartesian proj(pos.lat, pos.lon, pos_elevation);

    MapPixelDeltaInt output_size(360*20, 180*20);
    MapRegion result(std::shared_ptr<unsigned int>(
                     new unsigned int[output_size.x*output_size.y](),
                     ArrayDeleter<unsigned int>()),
             output_size.x, output_size.y);
    unsigned int *dest = result.GetRawData();
    ClippedRect(dest, output_size, MapPixelCoordInt(0, 0),
                MapPixelCoordInt(output_size.x - 1, output_size.y - 1),
                0xFFDDDDFF);

    PanoramaList pano_init;
    std::vector<PanoramaList> highpoints(output_size.x, pano_init);

    MapPixelCoordInt tile_center(mp_pos, TILE_SIZE);
    MapPixelDeltaInt tile_size(TILE_SIZE, TILE_SIZE);
    for (int tile_y = -5; tile_y <= 5; tile_y++) {
        for (int tile_x = -5; tile_x <= 5; tile_x++) {
            MapPixelCoordInt tile_coord(tile_center.x + tile_x*TILE_SIZE,
                                        tile_center.y + tile_y*TILE_SIZE);
            if (!CalcPanoramaOneTile(map, proj, tile_coord, tile_size,
                                     highpoints, dest, output_size))
            {
                return MapRegion();
            }
        }
    }
    for (auto xit = highpoints.begin(); xit != highpoints.end(); ++xit) {
        int pixel_x = xit - highpoints.begin();
        std::sort(xit->points.begin(), xit->points.end(),
                  [](const PanoramaListEntry &lhs, const PanoramaListEntry &rhs) {
                      return lhs.distance < rhs.distance;
                  });
        int cur_hi = std::numeric_limits<int>::max();
        for (auto yit = xit->points.cbegin(); yit != xit->points.cend(); ++yit) {
            if (yit->y > cur_hi)
                continue;
            cur_hi = yit->y;
            unsigned char color = round_to_int(yit->distance / 100000.0 * 255);
            ClippedSetPixel(dest, output_size, pixel_x, yit->y,
                            makeRGB(color, color, color, 1));
        }
    }
    ClippedLine(dest, output_size,
                MapPixelCoordInt(0, output_size.y/2),
                MapPixelCoordInt(output_size.x, output_size.y / 2),
                0xFFFF0000);
    SaveBufferAsBMP(L"test_panorama.bmp", dest, output_size.x, output_size.y, 32);
    return result;
}


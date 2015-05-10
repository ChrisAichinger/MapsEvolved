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
#include <iostream>
#include <stdio.h>

#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/LocalCartesian.hpp>

#include "util.h"
#include "map_geotiff.h"
#include "map_gvg.h"
#include "map_dhm_advanced.h"
#include "map_composite.h"
#include "bezier.h"
#include "projection.h"


GeoPixels::~GeoPixels() {};
GeoDrawable::~GeoDrawable() {};
RasterMap::~RasterMap() {};

PixelBuf EXPORT GetRegion_BoundsHelper(const GeoDrawable &drawable,
                                       const MapPixelCoordInt &pos,
                                       const MapPixelDeltaInt &size)
{
    MapPixelCoordInt endpos = pos + size;
    if (endpos.x <= 0 || endpos.y <= 0 ||
        pos.x >= static_cast<int>(drawable.GetWidth()) ||
        pos.y >= static_cast<int>(drawable.GetHeight()))
    {
        // Requested region fully out of bounds.
        return PixelBuf(size.x, size.y);
    }
    if (pos.x >= 0 && pos.y >= 0 &&
        endpos.x <= static_cast<int>(drawable.GetWidth()) &&
        endpos.y <= static_cast<int>(drawable.GetHeight()))
    {
        // Requested region fully in-bounds.
        return PixelBuf();
    }
    // Crop the request size and defer to the original GetRegion(), then copy
    // the result to the appropriate place in the output.
    auto newpos = MapPixelCoordInt(std::max(pos.x, 0), std::max(pos.y, 0));
    auto newend = MapPixelCoordInt(
            std::min(endpos.x, static_cast<int>(drawable.GetWidth())),
            std::min(endpos.y, static_cast<int>(drawable.GetHeight())));
    auto newsize = newend - newpos;
    auto pos_offset = PixelBufCoord(newpos.x - pos.x,
                                    size.y - newsize.y - (newpos.y - pos.y));

    auto result = PixelBuf(size.x, size.y);
    auto pixels = drawable.GetRegion(newpos, newsize);
    result.Insert(pos_offset, pixels);
    return result;
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
        virtual PixelBuf
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &sz) const
        {
            return PixelBuf(sz.x, sz.y);
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
        virtual const std::wstring &GetTitle() const { return m_fname; }
        virtual const std::wstring &GetDescription() const {
            return m_desc;
        }
        virtual ODMPixelFormat GetPixelFormat() const { return ODM_PIX_RGBX4; }

    private:
        const std::wstring m_fname;
        const std::wstring m_desc;
};


EXPORT std::shared_ptr<RasterMap> LoadMap(const std::wstring &fname) {
    std::wstring fname_lower(fname);
    std::transform(fname_lower.begin(), fname_lower.end(),
                   fname_lower.begin(), ::towlower);

    std::shared_ptr<RasterMap> map;
    try {
        if (starts_with(fname_lower, L"composite_map:")) {
            // A composite map.
            map.reset(new CompositeMap(fname));
            return map;
        }
        if (ends_with(fname_lower, L".tif") ||
            ends_with(fname_lower, L".tiff"))
        {
            // Geotiff file
            map.reset(new TiffMap(fname.c_str()));
            return map;
        } else if (ends_with(fname_lower, L".gvg"))
        {
            // GVG file
            map.reset(new GVGMap(fname));
            return map;
        } else {
            assert(false);  // Not implemented
        }
    } catch (const std::runtime_error &err) {
        std::wcerr << L"Could not open map '" << fname << L"'." << std::endl;
        std::wcerr << L"Error message: "
                   << WStringFromString(err.what(), "utf-8") << std::endl;
        map.reset(new RasterMapError(fname.c_str(), L"Exception"));
    }
    return map;
}

EXPORT std::vector<std::shared_ptr<RasterMap> >
AlternateMapViews(const std::shared_ptr<RasterMap> &map)
{
    std::vector<std::shared_ptr<RasterMap> > representations;
    if (map->GetType() == RasterMap::TYPE_DHM) {
        std::shared_ptr<class RasterMap> deriv_map(new GradientMap(map));
        representations.push_back(deriv_map);
        deriv_map.reset(new SteepnessMap(map));
        representations.push_back(deriv_map);
    }
    return representations;
}


bool CalcTerrainInfo(const std::shared_ptr<class RasterMap> &map,
                     const LatLon &pos, TerrainInfo *result)
{
    assert(result);

    MapPixelCoord map_pos;
    if (!map->LatLonToPixel(pos, &map_pos)) {
        return false;
    }
    MapBezierPositioner bezier_pos(map_pos, map->GetSize());
    if (!bezier_pos.IsValid()) {
        return false;
    }
    double mpp;
    if (!MetersPerPixel(map, bezier_pos.GetBezierCenter(), &mpp)) {
        return false;
    }
    unsigned int bezier_pixels = Bezier::N_POINTS - 1;
    double bezier_meters = bezier_pixels * mpp;

    MapBezierGradient grad;
    if (!Gradient3x3(*map, bezier_pos.GetBezierCenter(),
                     bezier_pos.GetBasePoint(), &grad))
    {
        return false;
    }

    grad /= bezier_meters;
    // -grad.y corrects for the coordinate system of the image pixels not being
    // the same as the map coordinates (y axis inverted)
    double grad_direction = atan2(-grad.y, grad.x);
    double grad_steepness = atan(grad.Abs());

    if (!Value3x3(*map, bezier_pos.GetBezierCenter(),
                  bezier_pos.GetBasePoint(), &result->height_m))
    {
        return false;
    }
    result->steepness_deg = grad_steepness * RAD_to_DEG;
    result->slope_face_deg = normalize_direction(270 +
                                                 grad_direction * RAD_to_DEG);
    return true;
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




MapPixelCoord MapPixelToMapPixel(const MapPixelCoord &pos,
                                 const GeoPixels &from_map,
                                 const GeoPixels &to_map)
{
    if (&from_map == &to_map) {
        return pos;
    }

    LatLon world_pos;
    if (!from_map.PixelToLatLon(pos, &world_pos)) {
        throw std::runtime_error(
                "MapPixelToMapPixel: Couldn't convert MapPixel to LatLon.");
    }

    MapPixelCoord to_pos;
    if (!to_map.LatLonToPixel(world_pos, &to_pos)) {
        throw std::runtime_error(
                "MapPixelToMapPixel: Couldn't convert LatLon to MapPixel.");
    }
    return to_pos;
}


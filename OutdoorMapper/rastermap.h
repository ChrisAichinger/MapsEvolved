#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"
#include "pixelbuf.h"



class EXPORT GeoPixels {
    // Georeferenced pixels. Subclasses of this type support mapping
    // pixel locations to world coordinates (LatLon) and vice versa.
    public:
        virtual ~GeoPixels();
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const = 0;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const = 0;
};


class EXPORT GeoDrawable : public GeoPixels {
    // Georeferenced drawables. Subclasses support one of two interfaces:
    // - DirectDrawing: GetRegionDirect() is used and must return a
    //   display-sized area. With this, GeoDrawables can draw directly to the
    //   screen, without additional resizing/rotations/stretching.
    //   This is used for gridlines and GPS tracks, for example.
    // - NonDirectDrawing: GetRegion() is used to get a tile from the
    //   underlaying map. MapDisplayManager takes care to display the resulting
    //   image on the screen in the right location, with the right size and
    //   orientation.
    //   This is used for normal rastermaps (topo maps and DHMs for example).
    public:
        enum DrawableType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT_MAP,
            TYPE_STEEPNESS_MAP,
            TYPE_LEGEND,
            TYPE_OVERVIEW,
            TYPE_IMAGE,
            TYPE_GPSTRACK,
            TYPE_GRIDLINES,
            TYPE_POI_DB,
            TYPE_ERROR,
        };
        virtual ~GeoDrawable();
        virtual DrawableType GetType() const = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual MapPixelDeltaInt GetSize() const = 0;

        // Get a specific area of the map.
        // pos: The topleft corner of the requested map area.
        // size: The dimensions of the requested map area.
        // The returned PixelBuf must have dimensions equal to size.
        virtual PixelBuf
        GetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const = 0;

        virtual Projection GetProj() const = 0;
        virtual const std::wstring &GetFname() const = 0;

        virtual bool IsViewable() const {
            return GetType() != TYPE_DHM && GetType() != TYPE_ERROR;
        };

        virtual bool SupportsDirectDrawing() const { return false; };
        virtual PixelBuf
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const
        {
            return PixelBuf();
        };
        virtual ODMPixelFormat GetPixelFormat() const = 0;
};


class EXPORT RasterMap : public GeoDrawable {
    // A GeoDrawable especially for maps. All logic was factored out to the
    // parent class.
    public:
        virtual ~RasterMap();
};


std::shared_ptr<RasterMap> EXPORT LoadMap(const std::wstring &fname);

EXPORT std::vector<std::shared_ptr<RasterMap> >
AlternateMapViews(const std::shared_ptr<RasterMap> &map);


struct EXPORT TerrainInfo {
    double height_m;
    double slope_face_deg;
    double steepness_deg;
};

class EXPORT HeightFinder {
    public:
        explicit HeightFinder();
        virtual ~HeightFinder() { m_active_dhm = nullptr; }
        virtual bool CalcTerrain(const LatLon &pos, TerrainInfo *result);
        std::shared_ptr<class RasterMap> GetActiveMap() const {
            return m_active_dhm;
        }
    protected:
        std::shared_ptr<class RasterMap> m_active_dhm;

        virtual bool LatLongWithinActiveDHM(const LatLon &pos) const;
        virtual std::shared_ptr<class RasterMap> FindBestMap(
                const LatLon &pos, GeoDrawable::DrawableType type) const;
};

bool EXPORT
GetMapDistance(const std::shared_ptr<class GeoDrawable> &map,
               const MapPixelCoord &pos,
               double dx, double dy, double *distance);
bool EXPORT
MetersPerPixel(const std::shared_ptr<class GeoDrawable> &map,
               const MapPixelCoord &pos,
               double *mpp);
bool EXPORT
MetersPerPixel(const std::shared_ptr<class GeoDrawable> &map,
               const MapPixelCoordInt &pos,
               double *mpp);

bool EXPORT
GetMapDistance(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoord &pos,
               double dx, double dy, double *distance);
bool EXPORT
MetersPerPixel(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoord &pos,
               double *mpp);
bool EXPORT
MetersPerPixel(const std::shared_ptr<class RasterMap> &map,
               const MapPixelCoordInt &pos,
               double *mpp);

PixelBuf EXPORT
CalcPanorama(const std::shared_ptr<GeoDrawable> &map, const LatLon &pos);

#endif

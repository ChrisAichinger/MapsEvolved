#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"
#include "pixelformat.h"

class EXPORT MapRegion {
    public:
        MapRegion() : m_data(), m_width(0), m_height(0) {};
        MapRegion(const std::shared_ptr<unsigned int> &data,
                  int width, int height)
            : m_data(data), m_width(width), m_height(height) {};
        inline std::shared_ptr<unsigned int> &GetData() { return m_data; }
        inline unsigned int * GetRawData() { return m_data.get(); }
        inline unsigned int GetWidth() const { return m_width; }
        inline unsigned int GetHeight() const { return m_height; }
        inline unsigned int GetPixel(int x, int y) {
            return m_data.get()[x + y*m_height];
        }
    private:
        std::shared_ptr<unsigned int> m_data;
        unsigned int m_width;
        unsigned int m_height;
};


class EXPORT GeoPixels {
    public:
        virtual ~GeoPixels();
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const = 0;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const = 0;
};


class EXPORT GeoDrawable : public GeoPixels {
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
        virtual MapRegion
        GetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const = 0;

        virtual Projection GetProj() const = 0;
        virtual const std::wstring &GetFname() const = 0;

        virtual bool IsViewable() const {
            return GetType() != TYPE_DHM && GetType() != TYPE_ERROR;
        };

        virtual bool SupportsDirectDrawing() const { return false; };
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const
        {
            return MapRegion();
        };
        virtual ODMPixelFormat GetPixelFormat() const = 0;
};


class EXPORT RasterMap : public GeoDrawable {
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

MapRegion EXPORT
CalcPanorama(const std::shared_ptr<GeoDrawable> &map, const LatLon &pos);

#endif

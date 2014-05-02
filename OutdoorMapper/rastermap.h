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


class EXPORT RasterMapCollection {
    public:
        RasterMapCollection() {};
        virtual ~RasterMapCollection();
        virtual void AddMap(const std::shared_ptr<RasterMap> &map) = 0;
        virtual void DeleteMap(unsigned int index) = 0;
        virtual size_t Size() const = 0;
        virtual std::shared_ptr<RasterMap> Get(size_t i) const = 0;
        virtual const std::vector<const std::shared_ptr<RasterMap> >
            GetAlternateRepresentations(size_t i) const = 0;
        virtual bool
            IsToplevelMap(const std::shared_ptr<RasterMap> &map) const = 0;

        virtual bool
            StoreTo(const std::unique_ptr<PersistentStore> &store) const = 0;
        virtual bool
            RetrieveFrom(const std::unique_ptr<PersistentStore> &store) = 0;
    private:
        DISALLOW_COPY_AND_ASSIGN(RasterMapCollection);
};

class EXPORT DefaultRasterMapCollection : public RasterMapCollection {
    public:
        DefaultRasterMapCollection();
        void AddMap(const std::shared_ptr<RasterMap> &map);
        void DeleteMap(unsigned int index);
        size_t Size() const {
            return m_maps.size();
        }
        std::shared_ptr<RasterMap> Get(size_t i) const {
            return m_maps[i].map;
        }
        const std::vector<const std::shared_ptr<RasterMap> >
        GetAlternateRepresentations(size_t i) const {
            const std::vector<const std::shared_ptr<RasterMap> > res(
                    m_maps[i].reps.cbegin(), m_maps[i].reps.cend());
            return res;
        }
        bool IsToplevelMap(const std::shared_ptr<RasterMap> &map) const;

        bool StoreTo(const std::unique_ptr<PersistentStore> &store) const;
        bool RetrieveFrom(const std::unique_ptr<PersistentStore> &store);
    private:
        struct MapAndReps {
            std::shared_ptr<RasterMap> map;
            std::vector<std::shared_ptr<RasterMap> > reps;
        };
        std::vector<MapAndReps> m_maps;

        DISALLOW_COPY_AND_ASSIGN(DefaultRasterMapCollection);
};

void EXPORT LoadMap(RasterMapCollection &maps, const std::wstring &fname);

struct EXPORT TerrainInfo {
    double height_m;
    double slope_face_deg;
    double steepness_deg;
};

class EXPORT HeightFinder {
    public:
        explicit HeightFinder(const class RasterMapCollection &maps);
        bool CalcTerrain(const LatLon &pos, TerrainInfo *result);
        std::shared_ptr<class RasterMap> GetActiveMap() const {
            return m_active_dhm;
        }
    private:
        const class RasterMapCollection &m_maps;
        std::shared_ptr<class RasterMap> m_active_dhm;

        bool LatLongWithinActiveDHM(const LatLon &pos) const;
        std::shared_ptr<class RasterMap> FindBestMap(
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

#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"

class ODM_INTERFACE RasterMap {
    public:
        enum RasterMapType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT,
            TYPE_LEGEND,
            TYPE_OVERVIEW,
            TYPE_ERROR,
            TYPE_IMAGE,
        };
        virtual ~RasterMap();

        virtual RasterMapType GetType() const = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual MapPixelDeltaInt GetSize() const = 0;
        virtual std::shared_ptr<unsigned int>
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const = 0;

        virtual bool PixelToPCS(double *x, double *y) const = 0;
        virtual bool PCSToPixel(double *x, double *y) const = 0;
        virtual Projection GetProj() const = 0;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const = 0;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const = 0;

        virtual const std::wstring &GetFname() const = 0;
};

class RasterMapCollection {
    public:
        RasterMapCollection();
        void AddMap(std::shared_ptr<RasterMap> map);
        void DeleteMap(unsigned int index);
        size_t Size() const {
            return m_maps.size();
        }
        const RasterMap &Get(size_t i) const {
            return *m_maps[i];
        }

        bool StoreTo(PersistentStore *store) const;
        bool RetrieveFrom(PersistentStore *store);
    private:
        std::vector<std::shared_ptr<RasterMap>> m_maps;

        DISALLOW_COPY_AND_ASSIGN(RasterMapCollection);
};

void LoadMap(RasterMapCollection &maps, const std::wstring &fname);

struct TerrainInfo {
    double height_m;
    double slope_face_deg;
    double steepness_deg;
};

class HeightFinder {
    public:
        explicit HeightFinder(const class RasterMapCollection &maps);
        bool CalcTerrain(const LatLon &pos, TerrainInfo *result);
    private:
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_active_dhm;

        bool LatLongWithinActiveDHM(const LatLon &pos) const;
        const class RasterMap *FindBestMap(const LatLon &pos,
                                       RasterMap::RasterMapType type) const;
};

bool GetMapDistance(const RasterMap &map, const MapPixelCoord &pos,
                    double dx, double dy, double *distance);
bool MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos,
                    double *mpp);
bool MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos,
                    double *mpp);

#endif

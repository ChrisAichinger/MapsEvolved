#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"
#include "coordinates.h"
#include "projection.h"

class EXPORT MapRegion {
    public:
        MapRegion(std::shared_ptr<unsigned int> &data, int width, int height)
            : m_data(data), m_width(width), m_height(height) {};
        inline std::shared_ptr<unsigned int> &GetData() { return m_data; }
        inline unsigned int * GetRawData() { return m_data.get(); }
        inline unsigned int GetWidth() const { return m_width; }
        inline unsigned int GetHeight() const { return m_height; }
        inline unsigned int GetPixel(int x, int y) { return m_data.get()[x + y*m_height]; }
    private:
        std::shared_ptr<unsigned int> m_data;
        unsigned int m_width;
        unsigned int m_height;
};

typedef std::shared_ptr<unsigned int> UIntShPtr;

class EXPORT ODM_INTERFACE RasterMap {
    public:
        enum RasterMapType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT,
            TYPE_STEEPNESS,
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
        virtual MapRegion
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

        virtual bool IsViewable() const {
            return GetType() != TYPE_DHM && GetType() != TYPE_ERROR;
        };
};

typedef std::shared_ptr<RasterMap> RasterMapShPtr;
typedef std::shared_ptr<const RasterMap> RasterMapShPtrConst;

class EXPORT RasterMapCollection {
    public:
        RasterMapCollection();
        void AddMap(std::shared_ptr<RasterMap> map);
        void DeleteMap(unsigned int index);
        size_t Size() const {
            return m_maps.size();
        }
        const RasterMap &Get(size_t i) const {
            return *m_maps[i].map;
        }
        std::shared_ptr<const RasterMap> GetSharedPtr(size_t i) const {
            return m_maps[i].map;
        }
        const std::vector<const std::shared_ptr<const RasterMap> >
        GetAlternateRepresentations(size_t i) const {
            const std::vector<const std::shared_ptr<const RasterMap> > res(m_maps[i].reps.cbegin(),
                                                                           m_maps[i].reps.cend());
            return res;
        }
        bool IsToplevelMap(const std::shared_ptr<const RasterMap> &map) const;

        bool StoreTo(PersistentStore *store) const;
        bool RetrieveFrom(PersistentStore *store);
    private:
        struct MapAndReps {
            std::shared_ptr<RasterMap> map;
            std::vector<std::shared_ptr<RasterMap> > reps;
        };
        std::vector<MapAndReps> m_maps;

        DISALLOW_COPY_AND_ASSIGN(RasterMapCollection);
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
    private:
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_active_dhm;

        bool LatLongWithinActiveDHM(const LatLon &pos) const;
        const class RasterMap *FindBestMap(const LatLon &pos,
                                       RasterMap::RasterMapType type) const;
};

bool EXPORT
    GetMapDistance(const RasterMap &map, const MapPixelCoord &pos,
                   double dx, double dy, double *distance);
bool EXPORT
    MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos,
                   double *mpp);
bool EXPORT
    MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos,
                   double *mpp);

#endif

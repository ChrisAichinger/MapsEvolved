#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "odm_config.h"
#include "util.h"

class ODM_INTERFACE RasterMap {
    public:
        enum RasterMapType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT,
            TYPE_LEGEND,
            TYPE_OVERVIEW,
        };
        virtual ~RasterMap();

        virtual RasterMapType GetType() const = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual std::shared_ptr<unsigned int> GetRegion(int x, int y,
                                        unsigned int width,
                                        unsigned int height) const = 0;

        virtual void PixelToPCS(double *x, double *y) const = 0;
        virtual void PCSToPixel(double *x, double *y) const = 0;
        virtual const class Projection &GetProj() const = 0;
        virtual void PixelToLatLong(double *x, double *y) const = 0;
        virtual void LatLongToPixel(double *x, double *y) const = 0;

        virtual const std::wstring &GetFname() const = 0;
};

class RasterMapCollection {
    public:
        RasterMapCollection();
        void AddMap(std::shared_ptr<RasterMap> map);
        size_t Size() const {
            return m_maps.size();
        }
        const RasterMap &Get(size_t i) const {
            return *m_maps[i];
        }
    private:
        DISALLOW_COPY_AND_ASSIGN(RasterMapCollection);
        std::vector<std::shared_ptr<RasterMap>> m_maps;
};

void LoadMap(RasterMapCollection &maps, const std::wstring &fname);

struct TerrainInfo {
    double height;
    double slope_face;
    double steepness_deg;
};

class HeightFinder {
    public:
        explicit HeightFinder(const class RasterMapCollection &maps);
        bool CalcTerrain(double lat, double lon, double *height,
                         double *slope_face, double *steepness_deg);
        bool CalcTerrain(double lat, double lon, TerrainInfo *result);
    private:
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_active_dhm;

        bool LatLongWithinActiveDHM(double x, double y) const;
        const class RasterMap *FindBestMap(double latitude, double longitude,
                                           RasterMap::RasterMapType type) const;
};

double GetMapDistance(const RasterMap &map, double Cx, double Cy, double dx, double dy);
double MetersPerPixel(const RasterMap *map, double x_px, double y_px);

#endif

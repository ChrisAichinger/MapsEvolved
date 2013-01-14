#ifndef ODM__RASTERMAP_H
#define ODM__RASTERMAP_H

#include <string>
#include <memory>
#include <vector>

#include "util.h"

class RasterMap {
    public:
        enum RasterMapType {
            TYPE_MAP = 1,
            TYPE_DHM,
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
        const RasterMap &GetDefaultMap() const { 
            return *m_maps[m_main_idx].get();
        }
        void AddMap(std::shared_ptr<RasterMap> map);
        size_t Size() const {
            return m_maps.size();
        }
        const RasterMap &Get(size_t i) const {
            return *m_maps[i];
        }
        void SetMain(size_t i) {
            m_main_idx = i;
        }
    private:
        DISALLOW_COPY_AND_ASSIGN(RasterMapCollection);
        unsigned int m_main_idx;
        std::vector<std::shared_ptr<RasterMap>> m_maps;
};

std::shared_ptr<class RasterMap> LoadMap(const std::wstring &fname);

class HeightFinder {
    public:
        explicit HeightFinder(const class RasterMapCollection &maps);
        double GetHeight(double latitude, double longitude);
    private:
        const class RasterMapCollection &m_maps;
        const class RasterMap *m_active_dhm;

        bool LatLongWithinActiveDHM(double x, double y) const;
        void LoadBestDHM(double latitude, double longitude);
};

#endif

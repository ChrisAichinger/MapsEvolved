#ifndef ODM__MAP_DHM_ADVANCED_H
#define ODM__MAP_DHM_ADVANCED_H

#include "rastermap.h"

class GradientMap : public RasterMap {
    public:
        explicit GradientMap(const std::shared_ptr<RasterMap> &orig_map);
        virtual RasterMap::RasterMapType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual bool PixelToPCS(double *x, double *y) const;
        virtual bool PCSToPixel(double *x, double *y) const;
        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;

        virtual const std::wstring &GetFname() const;
    private:
        const std::shared_ptr<RasterMap> m_orig_map;
};

class SteepnessMap : public RasterMap {
    public:
        explicit SteepnessMap(const std::shared_ptr<RasterMap> &orig_map);
        virtual RasterMap::RasterMapType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual bool PixelToPCS(double *x, double *y) const;
        virtual bool PCSToPixel(double *x, double *y) const;
        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;

        virtual const std::wstring &GetFname() const;
    private:
        const std::shared_ptr<RasterMap> m_orig_map;
};
#endif

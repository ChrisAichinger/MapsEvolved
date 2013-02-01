#ifndef ODM__MAP_GEOTIFF_H
#define ODM__MAP_GEOTIFF_H

#include "rastermap.h"

class TiffMap : public RasterMap {
    public:
        explicit TiffMap(const wchar_t *fname);
        virtual RasterMap::RasterMapType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDelta GetSize() const;
        virtual std::shared_ptr<unsigned int>
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual void PixelToPCS(double *x, double *y) const;
        virtual void PCSToPixel(double *x, double *y) const;
        virtual const class Projection &GetProj() const;

        virtual LatLon PixelToLatLon(const MapPixelCoord &pos) const;
        virtual MapPixelCoord LatLonToPixel(const LatLon &pos) const;

        virtual const std::wstring &GetFname() const;
    private:
        std::shared_ptr<class GeoTiff> m_geotiff;
        std::shared_ptr<class Projection> m_proj;
};

#endif

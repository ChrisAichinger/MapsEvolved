#ifndef ODM__MAP_GEOTIFF_H
#define ODM__MAP_GEOTIFF_H

#include "rastermap.h"

class TiffMap : public RasterMap {
    public:
        explicit TiffMap(const wchar_t *fname);
        virtual RasterMap::RasterMapType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual std::shared_ptr<unsigned int> GetRegion(
                int x, int y,
                unsigned int width, unsigned int height) const;

        virtual void PixelToPCS(double *x, double *y) const;
        virtual void PCSToPixel(double *x, double *y) const;
        virtual const class Projection &GetProj() const;

        virtual void PixelToLatLong(double *x, double *y) const;
        virtual void LatLongToPixel(double *x, double *y) const;

        virtual const std::wstring &GetFname() const;
    private:
        std::shared_ptr<class GeoTiff> m_geotiff;
        std::shared_ptr<class Projection> m_proj;
};

#endif

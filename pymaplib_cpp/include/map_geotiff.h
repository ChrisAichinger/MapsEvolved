#ifndef ODM__MAP_GEOTIFF_H
#define ODM__MAP_GEOTIFF_H

#include "rastermap.h"
#include "projection.h"

class EXPORT TiffMap : public RasterMap {
    public:
        explicit TiffMap(const wchar_t *fname);
        virtual GeoDrawable::DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual PixelBuf
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;


        virtual Projection GetProj() const;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;

        virtual const std::wstring &GetFname() const;
        virtual const std::wstring &GetTitle() const;
        virtual const std::wstring &GetDescription() const;
        virtual ODMPixelFormat GetPixelFormat() const {
            return ODM_PIX_RGBX4;
        }
    private:
        const std::shared_ptr<class GeoTiff> m_geotiff;
        Projection m_proj;

        virtual bool PixelToPCS(double *x, double *y) const;
        virtual bool PCSToPixel(double *x, double *y) const;
};

#endif

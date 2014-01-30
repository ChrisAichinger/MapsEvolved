#ifndef ODM__MAP_GEOTIFF_H
#define ODM__MAP_GEOTIFF_H

#include "rastermap.h"
#include "projection.h"

class TiffMap : public RasterMap {
    public:
        explicit TiffMap(const wchar_t *fname);
        virtual GeoDrawable::DrawableType GetType() const;
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
        const std::shared_ptr<class GeoTiff> m_geotiff;
        Projection m_proj;
};

#endif

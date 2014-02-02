
class TiffMap : public RasterMap {
%TypeHeaderCode
#include "rastermap.h"
#include "map_geotiff.h"
%End
    public:
        explicit TiffMap(const wchar_t *fname /In/);
        virtual GeoDrawable::DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const /Factory/;

        virtual bool PixelToPCS(double *x /In,Out/, double *y /In,Out/) const;
        virtual bool PCSToPixel(double *x /In,Out/, double *y /In,Out/) const;
        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
};

class GradientMap : public RasterMap {
%TypeHeaderCode
#include "rastermap.h"
#include "map_dhm_advanced.h"
%End
    public:
        explicit GradientMap(const RasterMapShPtr &orig_map /KeepReference/);
        virtual GeoDrawable::DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const /Factory/;

        virtual bool PixelToPCS(double *x /In,Out/, double *y /In,Out/) const;
        virtual bool PCSToPixel(double *x /In,Out/, double *y /In,Out/) const;
        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
};

class SteepnessMap : public RasterMap {
%TypeHeaderCode
#include "rastermap.h"
#include "map_dhm_advanced.h"
%End
    public:
        explicit SteepnessMap(const RasterMapShPtr &orig_map /KeepReference/);
        virtual GeoDrawable::DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const /Factory/;

        virtual bool PixelToPCS(double *x /In,Out/, double *y /In,Out/) const;
        virtual bool PCSToPixel(double *x /In,Out/, double *y /In,Out/) const;
        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
};


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

        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
        virtual bool SupportsDirectDrawing() const;
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
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

        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
        virtual bool SupportsDirectDrawing() const;
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
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

        virtual Projection GetProj() const;

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const;

        virtual const std::wstring &GetFname() const;
        virtual bool SupportsDirectDrawing() const;
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
};

class Gridlines : public GeoDrawable {
%TypeHeaderCode
#include "map_gridlines.h"
%End
    public:
        Gridlines();
        virtual ~Gridlines();

        virtual GeoDrawable::DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual Projection GetProj() const;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;
        virtual const std::wstring &GetFname() const;

        virtual bool SupportsDirectDrawing() const;
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
};

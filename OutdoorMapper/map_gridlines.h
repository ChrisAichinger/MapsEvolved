#ifndef ODM__MAP_GRIDLINES_H
#define ODM__MAP_GRIDLINES_H

#include <vector>
#include <string>

#include "coordinates.h"
#include "rastermap.h"

class EXPORT Gridlines : public GeoDrawable {
    /* Grid lines overlay. Currently, we have 1°, 0.5° or 0.1° overlays,
     * depending on the resolution of the base map.
     */
    public:
        Gridlines();
        virtual ~Gridlines();

        virtual GeoDrawable::DrawableType GetType() const {
            return GeoDrawable::TYPE_GRIDLINES;
        }
        /* Gridlines doesn't really have a size, as we generate the required
         * tiles on the fly. Currently, we simply return 100 pixels per degree
         * times (360, 180) degrees to "fill" the whole earth.
         * In practice, this is only relevant for GetRegion(), which we don't
         * normally use because we support direct drawing.
         */
        virtual unsigned int GetWidth() const { return m_size.x; }
        virtual unsigned int GetHeight() const { return m_size.y; }
        virtual MapPixelDeltaInt GetSize() const { return m_size; }
        virtual PixelBuf
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual Projection GetProj() const;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;
        virtual const std::wstring &GetFname() const { return fname; }

        virtual bool SupportsDirectDrawing() const { return true; };
        virtual PixelBuf
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
        virtual ODMPixelFormat GetPixelFormat() const {
            return ODM_PIX_RGBA4;
        }
    private:
        MapPixelDeltaInt m_size;
        static std::wstring fname;

        bool PixelToPCS(double *x, double *y) const;
        bool PCSToPixel(double *x, double *y) const;
        double GetLineSpacing(double lat_degrees, double lon_degrees) const;
        bool BisectLine(PixelBuf& buf,
                        const MapPixelCoord &map_start,
                        const MapPixelCoord &map_end,
                        const LatLon &ll_start,
                        const LatLon &ll_end,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        double scale_factor) const;
        bool LatLonToScreen(const LatLon &latlon,
                            const GeoPixels &base,
                            const MapPixelCoord &base_tl,
                            double scale_factor,
                            MapPixelCoord *result) const;
};

#endif

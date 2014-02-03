#ifndef ODM__GPSTRACKS_H
#define ODM__GPSTRACKS_H

#include <vector>
#include <string>

#include "coordinates.h"
#include "rastermap.h"

class EXPORT GPSSegment : public GeoDrawable {
    public:
        GPSSegment(const std::wstring &fname,
                   const std::vector<LatLon> &points);
        virtual ~GPSSegment();
        virtual const std::vector<LatLon> &GetPoints() const {
            return m_points;
        }

        virtual GeoDrawable::DrawableType GetType() const {
            return GeoDrawable::TYPE_GPSTRACK;
        }
        virtual unsigned int GetWidth() const { return m_size.x; }
        virtual unsigned int GetHeight() const { return m_size.y; }
        virtual MapPixelDeltaInt GetSize() const { return m_size; }
        virtual MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual Projection GetProj() const;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;
        virtual const std::wstring &GetFname() const { return m_fname; }

        virtual bool SupportsDirectDrawing() const { return true; };
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
    private:
        std::wstring m_fname;
        std::vector<LatLon> m_points;
        double m_lat_min, m_lat_max, m_lon_min, m_lon_max;
        MapPixelDeltaInt m_size;

        virtual bool PixelToPCS(double *x, double *y) const;
        virtual bool PCSToPixel(double *x, double *y) const;
};

class GPSOverlay {
    public:
        virtual ~GPSOverlay();
};
#endif

#ifndef ODM__MAP_DHM_ADVANCED_H
#define ODM__MAP_DHM_ADVANCED_H

#include "rastermap.h"

/** Construct a 3D gradient map from a DEM
 *
 * @locking Although concurrent `GetRegion` calls are enabled, no locking is
 * performed. Requests are passed to the DEM and the results are transformed
 * into a color image without modification of per-instance state.
 */
class EXPORT GradientMap : public RasterMap {
    public:
        explicit GradientMap(const std::shared_ptr<RasterMap> &orig_map);
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
        virtual bool SupportsConcurrentGetRegion() const {
            return m_orig_map->SupportsConcurrentGetRegion();
        }
    private:
        const std::shared_ptr<RasterMap> m_orig_map;
};

/** Construct a steepness map from a DEM
 *
 * @locking Although concurrent `GetRegion` calls are enabled, no locking is
 * performed. Requests are passed to the DEM and the results are transformed
 * into a color image without modification of per-instance state.
 */
class EXPORT SteepnessMap : public RasterMap {
    public:
        explicit SteepnessMap(const std::shared_ptr<RasterMap> &orig_map);
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
        virtual bool SupportsConcurrentGetRegion() const {
            return m_orig_map->SupportsConcurrentGetRegion();
        }
    private:
        const std::shared_ptr<RasterMap> m_orig_map;
};
#endif

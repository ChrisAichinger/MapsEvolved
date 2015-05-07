#ifndef ODM__MAP_COMBINED_H
#define ODM__MAP_COMBINED_H

#include <vector>
#include <memory>
#include <tuple>
#include <string>

#include "coordinates.h"
#include "rastermap.h"


/** A composite of multiple, smaller maps
 *
 * Present multiple RasterMaps collectively as one map. The individual maps
 * are treated like tiles and placed directly next to each other.
 *
 * The mystery of has_overlap_pixel (aka m_overlap_pixel):
 * Map tiles come in two variants:
 * 1) Tiles that can be placed next to each other to recreate a larger image.
 *    This is typical for tiles produced by cutting up a rastermap; each row
 *    and column from the original map is included in exactly one tile and
 *    never duplicated.
 *    This is also encountered with OSM tiles, as they are designed to fit
 *    together seamlessly to make the tile borders vanish.
 * 2) Tiles that overlap by one pixel with the adjacent tiles.
 *    This is common with tiles without projection ("geographic" projection /
 *    "+proj=latlong") with a defined resolution (degree/pixel), such as
 *    SRTM and other global DHM datasets.
 *    For example, SRTM3 tiles span 1° and contain 1201 pixels. The first pixel
 *    lies directly at e.g. 15.0000°, the last pixel at exactly 16.0000°. This
 *    results in each pixel representing exactly 3 arc seconds. Obviously, the
 *    last pixel overlaps with the next tile spanning 16.0000° - 17.0000°.
 *
 * Consequently, type (1) tiles must be placed next to each other to recreate
 * the original image, while type (2) tiles must be overlapped by one pixel.
 * To do this, we pretend that each tile is actually one pixel smaller (we
 * "clip" the right-most column and bottom-most row). This is handled via
 * SubmapWidth()/SubmapHeight().
 *
 * So, for type (1) tiles, has_overlap_pixel is false and m_overlap_pixel == 0.
 * So, for type (2) tiles, has_overlap_pixel is true and m_overlap_pixel == 1.
 *
 * @locking Although concurrent `GetRegion` calls are enabled, no locking is
 * performed. Requests are passed to the submaps and the results are stitched
 * together without modification of per-instance state.
 */
class EXPORT CompositeMap : public RasterMap {
    public:
        CompositeMap(unsigned int num_x, unsigned int num_y,
                     bool has_overlap_pixel,
                     const std::vector<std::shared_ptr<RasterMap>> &submaps);
        CompositeMap(const std::wstring &fname_token);
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
        virtual bool SupportsConcurrentGetRegion() const;
        static std::vector<std::wstring>
        ParseFname(const std::wstring &fname,
                   unsigned int *num_maps_x, unsigned int *num_maps_y,
                   bool *has_overlap_pixel);
        static std::vector<std::shared_ptr<RasterMap>>
        LoadFnameMaps(const std::wstring &fname,
                      unsigned int *num_maps_x, unsigned int *num_maps_y,
                      bool *has_overlap_pixel);
        static std::wstring
        FormatFname(unsigned int num_maps_x, unsigned int num_maps_y,
                    bool has_overlap_pixel,
                    const std::vector<std::wstring> &fnames);
        static std::wstring
        FormatFname(unsigned int num_maps_x, unsigned int num_maps_y,
                    bool has_overlap_pixel,
                    const std::vector<std::shared_ptr<RasterMap>> &orig_maps);
        static std::wstring
        FormatFname(const class CompositeMap &this_map);
    private:
        void init();
        inline unsigned int index(unsigned int x, unsigned int y) const {
            return x + y * m_num_x;
        }
        template <typename T>
        std::tuple<unsigned int, unsigned int, T>
        find_submap(T coord) const;

        unsigned int SubmapWidth(unsigned int mx, unsigned int my) const;
        unsigned int SubmapHeight(unsigned int mx, unsigned int my) const;

        unsigned int m_num_x, m_num_y;
        int m_overlap_pixel;

        std::vector<std::shared_ptr<RasterMap> > m_submaps;
        unsigned int m_width, m_height;

        std::wstring m_fname;
        std::wstring m_title;
        std::wstring m_description;

        bool m_concurrent_getregion;
};

#endif

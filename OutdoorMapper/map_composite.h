#ifndef ODM__MAP_COMBINED_H
#define ODM__MAP_COMBINED_H

#include <vector>
#include <memory>
#include <tuple>
#include <string>

#include "coordinates.h"
#include "rastermap.h"

class EXPORT CompositeMap : public RasterMap {
    public:
        CompositeMap(unsigned int num_x, unsigned int num_y,
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
        virtual ODMPixelFormat GetPixelFormat() const {
            return ODM_PIX_RGBX4;
        }
        static std::vector<std::wstring>
        ParseFname(const std::wstring &fname,
                   unsigned int *num_maps_x, unsigned int *num_maps_y);
        static std::vector<std::shared_ptr<RasterMap>>
        LoadFnameMaps(const std::wstring &fname,
                      unsigned int *num_maps_x, unsigned int *num_maps_y);
        static std::wstring
        FormatFname(unsigned int num_maps_x, unsigned int num_maps_y,
                    const std::vector<std::wstring> &fnames);
        static std::wstring
        FormatFname(unsigned int num_maps_x, unsigned int num_maps_y,
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

        unsigned int m_num_x, m_num_y;

        std::vector<std::shared_ptr<RasterMap> > m_submaps;
        unsigned int m_width, m_height;

        std::wstring m_fname;
};

#endif

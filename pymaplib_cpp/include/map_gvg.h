#ifndef ODM__MAP_GVG_H
#define ODM__MAP_GVG_H

#include <string>
#include <vector>
#include <tuple>
#include <iostream>
#include <fstream>
#include <cstdint>

#include <boost/thread/mutex.hpp>

#include "odm_config.h"
#include "pixelbuf.h"
#include "rastermap.h"

struct EXPORT GVGHeader {
    float FileVersion;
    unsigned int VendorCode;
    unsigned int ProductCode;
    std::wstring CopyrightInfo;
    std::wstring LicenseInfo;
    std::wstring MapInfo;
    std::wstring Title;
    std::wstring Description;
    std::wstring AutoLayer;
    std::wstring HideLayer;
    bool AutoSwitch;
    bool AutoTile;
    float ObjectScale;
    bool CacheMode;
    bool AutoFrame;
    unsigned int BkColor;
    bool StretchNice;
    std::wstring Gauges;

    GVGHeader() :
        FileVersion(0), VendorCode(0), ProductCode(0), CopyrightInfo(),
        LicenseInfo(), MapInfo(), Title(), Description(), AutoLayer(),
        HideLayer(), AutoSwitch(0), AutoTile(0), ObjectScale(0),
        CacheMode(0), AutoFrame(0), BkColor(0), StretchNice(0), Gauges()
        { };
    bool set_field(const std::wstring &key, const std::wstring &value);

    // Get the number of resolution steps (GVGMapInfo structures).
    unsigned int count_gauges();
};

struct EXPORT GVGMapInfo {
    std::wstring Type;
    std::wstring Path;
    std::wstring Brightness;
    int Scale;
    std::wstring Ellipsoid;
    std::wstring Projection;
    double BaseMed;
    int Zone;
    double OffsetEast;
    double OffsetNorth;
    double WorldOrgX;
    double WorldOrgY;
    double WPPX;
    double WPPY;
    double RADX;
    double RADY;
    unsigned int ImageWidth;
    unsigned int ImageHeight;
    std::wstring BorderPly;
    std::wstring LegendImage;

    double RADX_sin, RADX_cos, RADY_sin, RADY_cos;

    GVGMapInfo() :
        Type(), Path(), Brightness(), Scale(0), Ellipsoid(),
        Projection(), BaseMed(0), Zone(0), OffsetEast(0), OffsetNorth(0),
        WorldOrgX(0), WorldOrgY(0), WPPX(0), WPPY(0), RADX(0), RADY(0),
        ImageWidth(0), ImageHeight(0), BorderPly(), LegendImage(),
        RADX_sin(0), RADX_cos(0), RADY_sin(0), RADY_cos(0)
        { };

    bool set_field(const std::wstring &key, const std::wstring &value);
    void complete_initialization();
    void Pixel_to_PCS(double x_px, double y_px,
                      double* x_pcs, double* y_pcs) const;
    void PCS_to_Pixel(double x_pcs, double y_pcs,
                      double* x_px, double* y_px) const;
};

class EXPORT GVGFile {
    public:
        explicit GVGFile(const std::wstring &fname);

        const std::wstring &Filename() const { return m_fname; };
        const GVGHeader &Header() const { return m_header; };
        unsigned int MapInfoCount() const { return m_mapinfos.size(); };
        const GVGMapInfo &MapInfo(unsigned int n) const;
        std::tuple<std::wstring, unsigned int> GmpPath(unsigned int n) const;

        const std::wstring &RawDataString() const { return m_decoded_data; };
        unsigned int BestResolutionIndex() const;

    private:
        std::wstring m_fname;
        GVGHeader m_header;
        std::vector<GVGMapInfo> m_mapinfos;
        std::wstring m_decoded_data;

        void ParseINI(const std::wstring &data);
        std::wstring ResolvePath(const std::wstring &gmp_name) const;

};

PACKED_STRUCT(
struct EXPORT GMPBitmapFileHdr {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
});

PACKED_STRUCT(
struct EXPORT GMPBitmapInfoHdr {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
});

PACKED_STRUCT(
struct GMPHeader {
    uint32_t unkn1;
    uint32_t unkn2;
    uint32_t compression;
    uint32_t tile_px_x;
    uint32_t tile_px_y;
    uint32_t unkn4;
});

PACKED_STRUCT(
struct GMPTileOffset {
    int64_t offset;
    int64_t length;
});

class EXPORT GMPImage {
    public:
        GMPImage(const std::wstring &fname, int index, int64_t foffset);
        GMPImage(const GMPImage &other);
        friend void swap(GMPImage &lhs, GMPImage &rhs) {
            using std::swap;  // Enable ADL.

            swap(lhs.m_file, rhs.m_file);
            swap(lhs.m_fname, rhs.m_fname);
            swap(lhs.m_findex, rhs.m_findex);
            swap(lhs.m_foffset, rhs.m_foffset);
            swap(lhs.m_bfh, rhs.m_bfh);
            swap(lhs.m_bih_buf, rhs.m_bih_buf);
            swap(lhs.m_bih, rhs.m_bih);
            swap(lhs.m_gmphdr, rhs.m_gmphdr);
            swap(lhs.m_tiles_x, rhs.m_tiles_x);
            swap(lhs.m_tiles_y, rhs.m_tiles_y);
            swap(lhs.m_tiles, rhs.m_tiles);
            swap(lhs.m_tile_index, rhs.m_tile_index);
            swap(lhs.m_topdown, rhs.m_topdown);
        }
        GMPImage& operator=(GMPImage other) {
            swap(*this, other);
            return *this;
        }

        PixelBuf LoadTile(long tx, long ty) const;
        std::string LoadCompressedTile(long tx, long ty) const;

        std::wstring DebugData() const;
        int AnnouncedWidth() const;
        int AnnouncedHeight() const;
        int RealWidth() const;
        int RealHeight() const;
        int TileWidth() const;
        int TileHeight() const;
        int NumTilesX() const;
        int NumTilesY() const;
        int BitsPerPixel() const;
        int64_t NextImageOffset() const;

    private:
        mutable std::ifstream m_file;
        std::wstring m_fname;
        int m_findex;
        int64_t m_foffset;

        GMPBitmapFileHdr m_bfh;
        std::string m_bih_buf;
        const struct GMPBitmapInfoHdr *m_bih;
        const GMPHeader *m_gmphdr;

        unsigned int m_tiles_x, m_tiles_y, m_tiles;
        std::vector<GMPTileOffset> m_tile_index;
        bool m_topdown;

        void Init();
        bool IsSupportedBPP(unsigned int bpp) const {
            return bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32;
        }
};

EXPORT GMPImage MakeGmpImage(const std::wstring& path,
                             unsigned int gmp_image_idx);
EXPORT GMPImage MakeBestResolutionGmpImage(const GVGFile &gvgfile);

/** A map in GVG file format
 *
 * GVG files can in principle contain both normal topographic data and DEM
 * data. DEM's are currently not supported, though.
 *
 * @locking Concurrent `GetRegion` calls are enabled. Data access in
 * `GetRegion` is protected by a per-instance mutex `m_getregion_mutex`.
 * No external calls are made with this mutex held.
 */
class EXPORT GVGMap : public RasterMap {
    public:
        explicit GVGMap(const std::wstring &fname);
        virtual ~GVGMap();

        virtual DrawableType GetType() const;
        virtual unsigned int GetWidth() const;
        virtual unsigned int GetHeight() const;
        virtual MapPixelDeltaInt GetSize() const;

        // Get a specific area of the map.
        // pos: The topleft corner of the requested map area.
        // size: The dimensions of the requested map area.
        // The returned PixelBuf must have dimensions equal to size.
        virtual PixelBuf
        GetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const;

        virtual Projection GetProj() const;
        virtual const std::wstring &GetFname() const;
        virtual const std::wstring &GetTitle() const;
        virtual const std::wstring &GetDescription() const;

        virtual bool IsViewable() const {
            return GetType() != TYPE_DHM && GetType() != TYPE_ERROR;
        };

        virtual bool SupportsDirectDrawing() const { return false; };
        virtual PixelBuf
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const
        {
            return PixelBuf();
        };

        virtual ODMPixelFormat GetPixelFormat() const;
        virtual bool SupportsConcurrentGetRegion() const { return true; }

        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const;

        const GVGFile &GetGVGFile() const { return m_gvgfile; };
        const GMPImage &GetGMPImage() const { return m_image; };
        const GVGHeader *GetGVGHeader() const { return m_gvgheader; };
        const GVGMapInfo *GetGVGMapInfo() const { return m_gvgmapinfo; };
    private:
        DISALLOW_COPY_AND_ASSIGN(GVGMap);

        mutable boost::mutex m_getregion_mutex;

        GVGFile m_gvgfile;
        GMPImage m_image;
        const GVGHeader* m_gvgheader;
        const GVGMapInfo* m_gvgmapinfo;

        // sizes
        long m_tile_width, m_tile_height;
        long m_tiles_x, m_tiles_y;
        long m_width, m_height;
        long m_tilesize, m_tilesizergb;

        // colors
        long m_bpp;

        std::string m_proj_str;
        Projection m_proj;

        std::string MakeProjString() const;
};

#endif

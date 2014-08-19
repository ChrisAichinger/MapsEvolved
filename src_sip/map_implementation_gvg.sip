
typedef unsigned int uint32_t;
typedef unsigned int uint16_t;
typedef int int32_t;

struct GMPHeader {
%TypeHeaderCode
#include "map_gvg.h"
%End
    uint32_t unkn1;
    uint32_t unkn2;
    uint32_t compression;
    uint32_t tile_px_x;
    uint32_t tile_px_y;
    uint32_t unkn4;
};

struct GMPBitmapFileHdr {
%TypeHeaderCode
#include "map_gvg.h"
%End
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};

struct GMPBitmapInfoHdr {
%TypeHeaderCode
#include "map_gvg.h"
%End
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
};

struct GVGHeader {
%TypeHeaderCode
#include "map_gvg.h"
%End
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
};

struct GVGMapInfo {
%TypeHeaderCode
#include "map_gvg.h"
%End
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


    void Pixel_to_PCS(double xpx, double ypx,
                      double* xout /Out/, double* yout /Out/) const;
    void PCS_to_Pixel(double x_coor, double y_coor,
                      double* xpx /Out/, double* ypx /Out/) const;
};

class GVGFile {
%TypeHeaderCode
#include "map_gvg.h"
%End
    public:
        explicit GVGFile(const std::wstring &fname);

        const std::wstring &Filename() const;
        const GVGHeader &Header() const;
        unsigned int MapInfoCount() const;
        const GVGMapInfo &MapInfo(unsigned int n) const;

        const std::wstring &RawDataString() const;
        unsigned int BestResolutionIndex() const;
};

class GMPImage {
%TypeHeaderCode
#include "map_gvg.h"
%End
    public:
        GMPImage(const std::wstring &fname, int index, int foffset);
        GMPImage(const GMPImage &other);

        PixelBuf LoadTile(int tx, int ty) const;
        std::string LoadCompressedTile(int tx, int ty) const;

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
};

GMPImage MakeGmpImage(const std::wstring& path,
                      unsigned int gmp_image_idx);
GMPImage MakeBestResolutionGmpImage(const GVGFile &gvgfile);


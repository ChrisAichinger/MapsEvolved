#include <string>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cpl_serv.h"
#include "tiff.h"
#include "libxtiff/xtiffio.h"
#include "geotiffio.h"
#include "geovalues.h"
#include "geo_normalize.h"

#include "rastermap.h"
#include "map_geotiff.h"
#include "projection.h"
#include "util.h"

const char * const DEFAULT_ENCODING = "UTF-8";

// Map between the unit square (x and y in [0, 1]) and a general
// quadrilateral using bilinear interpolation.
// Normalized coordinates are given as UnitSquareCoord.
// Quadrilateral coordinatese are given as MapPixelDelta.
class BilinearInterpolator {
    public:
        // Initialize with the four corners of the general quadrilateral.
        // The coordinates are given in clock-wise order.
        BilinearInterpolator(const MapPixelDelta &P00,
                             const MapPixelDelta &P10,
                             const MapPixelDelta &P11,
                             const MapPixelDelta &P01);

        // Map from input (within the unit square) to the general quad.
        // Example: forward(UnitSquareCoord(0.25, 0)) -> P00 * 0.75 + P10 * 0.25
        MapPixelDelta forward(const UnitSquareCoord &input);

        // Map from the general quad back to the unit square.
        // This is the inverse transformation of forward().
        UnitSquareCoord inverse(const MapPixelDelta &X);
    private:
        template <typename T>
        T lerp(double f, const T &A, const T &B) {
            return (1-f)*A + f*B;
        }
        const MapPixelDelta A, B, C, D;
        const MapPixelDelta E, F, G;
};

BilinearInterpolator::BilinearInterpolator(const MapPixelDelta &P00,
                                           const MapPixelDelta &P10,
                                           const MapPixelDelta &P11,
                                           const MapPixelDelta &P01)
: A(P00), B(P10), C(P11), D(P01), E(B-A), F(D-A), G(A-B-D+C)
{}


MapPixelDelta BilinearInterpolator::forward(const UnitSquareCoord &input) {
    // Straight-forward bilinear interpolation.
    return lerp(input.y, lerp(input.x, A, B), lerp(input.x, D, C));
}

UnitSquareCoord BilinearInterpolator::inverse(const MapPixelDelta &X) {
    // Inverse bilinear interpolation involves solving a quadratic equation.
    // Cf. http://www.iquilezles.org/www/articles/ibilinear/ibilinear.htm
    MapPixelDelta H = X-A;
    double k2 = G.x*F.y - G.y*F.x;
    double k1 = E.x*F.y - E.y*F.x + H.x*G.y - H.y*G.x;
    double k0 = H.x*E.y - H.y*E.x;
    double v = (-k1 + sqrt(k1*k1 - 4*k0*k2)) / (2 * k2);
    if (v < 0 || v > 1) {
        v = (-k1 - sqrt(k1*k1 - 4*k0*k2)) / (2 * k2);
    }
    double u = (H.x - F.x*v)/(E.x + G.x*v);
    return UnitSquareCoord(u, v);
}


class TiffHandle {
    public:
        explicit TiffHandle(const std::wstring &fname)
        // Pass "m" flag to disable memory-mapped IO.
        // Otherwise we fill up the whole virtual address space with mmapped
        // TIF files and run out of space to load libraries or map other data.
        // This gives hard-to-diagnose OOM errors (debug with VMMap on Win32).
        : m_tiff(XTIFFOpenW(fname.c_str(), "rm"))
        {
            if (!m_tiff) {
                throw std::runtime_error("File not found");
            }
        };
        ~TiffHandle() {
            if (m_tiff) {
                XTIFFClose(m_tiff);
                m_tiff = NULL;
            }
        }
        TIFF *GetTIFF() { return m_tiff; };
    private:
        DISALLOW_COPY_AND_ASSIGN(TiffHandle);
        TIFF* m_tiff;
};

class GeoTiffHandle {
    public:
        explicit GeoTiffHandle(TiffHandle &tiffhandle)
            : m_gtif(GTIFNew(tiffhandle.GetTIFF()))
        {
            if (!m_gtif)
                throw std::runtime_error("Opening GeoTiff failed.");
        };
        ~GeoTiffHandle() {
            if (m_gtif) {
                GTIFFree(m_gtif);
                m_gtif = NULL;
            }
        };
        GTIF *GetGTIF() { return m_gtif; };
    private:
        DISALLOW_COPY_AND_ASSIGN(GeoTiffHandle);
        GTIF *m_gtif;
};

class Tiff {
    public:
        explicit Tiff(const std::wstring &fname);
        virtual ~Tiff() { };
        TIFF *GetTIFF() { return m_rawtiff; };
        unsigned int GetWidth() const { return m_width; };
        unsigned int GetHeight() const { return m_height; };
        unsigned int GetBitsPerSample() const { return m_bitspersample; };
        unsigned int GetSamplesPerPixel() const { return m_samplesperpixel; };
        PixelBuf GetRegion(
                const class MapPixelCoordInt &pos,
                const class MapPixelDeltaInt &size) const;

        template <typename T>
        std::tuple<unsigned int, const T*>
        GetField(ttag_t field) const;

        const std::wstring &GetFilename() const { return m_fname; };
        const std::wstring &GetTitle() const { return m_title; };
    protected:
        const std::wstring m_fname;
        std::wstring m_title;
        TiffHandle m_tiffhandle;
        TIFF *m_rawtiff;

        virtual void Hook_TIFFRGBAImageGet(TIFFRGBAImage &img) const {};
    private:
        unsigned int m_width, m_height;
        unsigned short int m_bitspersample, m_samplesperpixel;

        PixelBuf DoGetRegion(
                const class MapPixelCoordInt &pos,
                const class MapPixelDeltaInt &size) const;
};

class GeoTiff : public Tiff {
    public:
        explicit GeoTiff(const std::wstring &fname);
        virtual ~GeoTiff() { };

        bool CheckVersion() const;
        bool LoadCoordinates();

        bool PixelToPCS(double *x, double *y) const;
        bool PCSToPixel(double *x, double *y) const;
        const std::string &GetProj4String() const { return m_proj; };
        GeoDrawable::DrawableType GetType() const { return m_type; };

        template <typename T>
        std::tuple<unsigned int, std::shared_ptr<T>>
        GetKey(geokey_t key) const;

        template <typename T>
        bool GetKeySingle(geokey_t key, T *result) const;

        bool HasKey(geokey_t) const;

        geocode_t GetModel() const { return m_model; }

    protected:
        GeoTiffHandle m_gtifhandle;
        GTIF *m_rawgtif;

        virtual void Hook_TIFFRGBAImageGet(TIFFRGBAImage &img) const;
    private:
        bool CheckDHMValid() const;

        geocode_t m_model;
        const double *m_tiepoints;
        const double *m_pixscale;
        const double *m_transform;
        unsigned short int m_ntiepoints, m_npixscale, m_ntransform;

        std::string m_proj;
        GeoDrawable::DrawableType m_type;
};


static void put16bitbw_DHM(
    TIFFRGBAImage* img,
    uint32* cp,                     // ptr to write target in TO (dest buf)
    uint32 x, uint32 y,             // x,y offset of write target within TO
    uint32 w, uint32 h,             // pixel count of one row; number of rows
    int32 fromskew, int32 toskew,   // offsets on row change in FROM/TO
    unsigned char* pp)              // ptr to read location in FROM
{
    int samplesperpixel = img->samplesperpixel;
    int16 *wp = reinterpret_cast<int16*>(pp);
    while (h-- > 0) {
        for (x = w; x-- > 0;) {
            // Copy the 16 bit heightmap value over into the RGBA image
            // Take care to sign-extend
            *cp++ = (uint32)*wp;
            wp += samplesperpixel;
        }
        cp += toskew;
        wp += fromskew;
    }
}


Tiff::Tiff(const std::wstring &fname)
    : m_fname(fname), m_title(), m_tiffhandle(fname),
      m_rawtiff(m_tiffhandle.GetTIFF())
{
    if (!TIFFGetField(m_rawtiff, TIFFTAG_IMAGEWIDTH, &m_width) ||
        !TIFFGetField(m_rawtiff, TIFFTAG_IMAGELENGTH, &m_height)) {
        throw std::runtime_error("Failed getting TIF dimensions.");
    }

    if (!TIFFGetField(m_rawtiff, TIFFTAG_BITSPERSAMPLE, &m_bitspersample) ||
        !TIFFGetField(m_rawtiff, TIFFTAG_SAMPLESPERPIXEL, &m_samplesperpixel))
    {
        throw std::runtime_error("Failed getting TIF pixel format.");
    }
    char *title;
    if (TIFFGetField(m_rawtiff, TIFFTAG_DOCUMENTNAME, &title)) {
        m_title = WStringFromString(title, DEFAULT_ENCODING);
    }
};

PixelBuf
Tiff::DoGetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const
{
    MapPixelCoordInt endpos = pos + size;
    if (endpos.x <= 0 || endpos.y <= 0 ||
        pos.x >= static_cast<int>(m_width) ||
        pos.y >= static_cast<int>(m_height))
    {
        return PixelBuf(size.x, size.y);
    }

    TIFFRGBAImage img;
    char emsg[1024] = "";
    if (!TIFFRGBAImageOK(m_rawtiff, emsg) ||
        !TIFFRGBAImageBegin(&img, m_rawtiff, 0, emsg)) {
        throw std::runtime_error("TIFF RGBA access not possible.");
    }

    img.col_offset = pos.x;
    img.row_offset = pos.y;

    // Give derived classes a chance to influence TIFFRGBAGetImage
    Hook_TIFFRGBAImageGet(img);

    PixelBuf result(size.x, size.y);
    int ok = TIFFRGBAImageGet(&img, result.GetRawData(), size.x, size.y);
    TIFFRGBAImageEnd(&img);

    if (!ok) {
        throw std::runtime_error("Loading TIFF data failed.");
    }
    return result;
}

PixelBuf
Tiff::GetRegion(const MapPixelCoordInt &pos,
                const MapPixelDeltaInt &size) const
{
    if (TIFFIsTiled(m_rawtiff))
        return DoGetRegion(pos, size);

    // Handle stripped images:
    // TIFFRGBAImageGet ignores img.col_offset for stripped images.
    // It always returns data starting from the first column of the image.
    //
    // So, fetch a full-width strip and copy the relevant portion into the
    // output PixelBuf.
    auto result = PixelBuf(size.x, size.y);
    MapPixelCoordInt end = pos + size;

    int strip_size = -1;
    if (!TIFFGetFieldDefaulted(m_rawtiff, TIFFTAG_ROWSPERSTRIP, &strip_size)) {
        throw std::runtime_error("Failed getting TIF dimensions.");
    }
    if (strip_size < 0) {
        throw std::runtime_error("Stripped TIF image with strip height < 0?!");
    }

    int first_ty = pos.y / strip_size;
    int last_ty = (end.y - 1) / strip_size;
    for (int ty = pos.y / strip_size; ty*strip_size < end.y; ty++) {
        PixelBuf tile = DoGetRegion(
                MapPixelCoordInt(0, ty*strip_size),
                MapPixelDeltaInt(m_width, strip_size));
        if (!tile.GetData()) {
            continue;
        }
        auto insert_pos = PixelBufCoord(
                -pos.x,
                (last_ty - ty + first_ty) * strip_size - pos.y);
        result.Insert(insert_pos, tile);
    }
    return result;
}

template <typename T>
std::tuple<unsigned int, const T*>
Tiff::GetField(ttag_t field) const {
    const T *data;
    unsigned int length;
    if (!TIFFGetField(m_rawtiff, field, &length, &data)) {
        length = 0;
        data = NULL;
    }
    return std::make_tuple(length, data);
}

static const char *CSVFileOverride(const char * pszInput) {
    static char szPath[1024];
    std::string csvdir = GetModuleDir_char() + "csv" + ODM_PathSep_char;
    sprintf_s(szPath, sizeof(szPath), "%s%s", csvdir.c_str(), pszInput);
    if (!FileExists(szPath)) {
        throw std::runtime_error(string_format("CSV file does not exist: %s",
                                               szPath));
    }
    return(szPath);
}

// http://rocky.ess.washington.edu/data/raster/geotiff/docs/manual.txt
// ftp://kratmos.gsfc.nasa.gov/pub/jim/imager/latest_version/TIFF_reader2.c
// http://svn.osgeo.org/fdocore/tags/3.2.x_G052/Thirdparty/GDAL1.3/src/frmts/gtiff/geotiff.cpp
// http://ojs.klaki.net/geotiff2ncdf/gt2nc.c

GeoTiff::GeoTiff(const std::wstring &fname)
    : Tiff(fname), m_gtifhandle(m_tiffhandle),
      m_rawgtif(m_gtifhandle.GetGTIF()),
      m_tiepoints(NULL), m_pixscale(NULL), m_transform(NULL),
      m_ntiepoints(0), m_npixscale(0), m_ntransform(0),
      m_proj(), m_type()
{
    SetCSVFilenameHook(&CSVFileOverride);
    if (!CheckVersion()) {
        throw std::runtime_error("GeoTIFF version not supported.");
    }
    if (!LoadCoordinates()) {
        m_type = RasterMap::TYPE_IMAGE;
    }
};

bool GeoTiff::CheckVersion() const {
    enum GTIFVersion { VERSION = 0, REV_MAJOR, REV_MINOR };
    int versions[3];
    GTIFDirectoryInfo(m_rawgtif, versions, NULL);
    return versions[VERSION] <= GvCurrentVersion &&
           versions[REV_MAJOR] <= GvCurrentRevision;
}

bool GeoTiff::LoadCoordinates() {
    if (!GetKeySingle<geocode_t>(GTModelTypeGeoKey, &m_model)) {
        return false;
    }

    // The rest is not implemented
    if (m_model != ModelTypeProjected && m_model != ModelTypeGeographic) {
        throw std::runtime_error("Map type not supported yet");
    }

    GTIFDefn    defn;
    if(!GTIFGetDefn(m_rawgtif, &defn)) {
        return false;
    }

    std::shared_ptr<char> proj_str(GTIFGetProj4Defn(&defn),
                                   [](char* mem) { GTIFFreeMemory(mem); });
    if (!proj_str)
        return false;

    m_proj = proj_str.get();
    if (!m_proj.length() || !m_proj[0])
        return false;

    using std::tie;
    tie(m_ntiepoints, m_tiepoints) = GetField<double>(TIFFTAG_GEOTIEPOINTS);
    tie(m_npixscale, m_pixscale) = GetField<double>(TIFFTAG_GEOPIXELSCALE);
    tie(m_ntransform, m_transform) = GetField<double>(TIFFTAG_GEOTRANSMATRIX);

    unsigned int sample_fmt = 0;
    TIFFGetFieldDefaulted(m_rawtiff, TIFFTAG_SAMPLEFORMAT, &sample_fmt);
    if (HasKey(VerticalUnitsGeoKey)) {
        m_type = RasterMap::TYPE_DHM;
    } else if (GetSamplesPerPixel() == 1 && GetBitsPerSample() == 16 &&
               sample_fmt == SAMPLEFORMAT_INT)
    {
        // Some DHM's don't set VerticalUnitsGeoKey, unfortunately.
        // Use this heuristic to catch those, SAMPLEFORMAT_INT should be
        // a pretty good indicator.
        m_type = RasterMap::TYPE_DHM;
    } else {
        m_type = RasterMap::TYPE_MAP;
    }

    return true;
}

template <typename T>
std::tuple<unsigned int, std::shared_ptr<T>>
GeoTiff::GetKey(geokey_t key) const
{
    int size;
    tagtype_t type;
    unsigned int count = GTIFKeyInfo(m_rawgtif, key, &size, &type);
    if (!count) {
        return make_tuple(0, std::shared_ptr<T>());
    }
    if (sizeof(T) != size) {
        throw std::runtime_error("GeoTIFF key malformed.");
    }

    std::shared_ptr<T> buffer(new T[count](), ArrayDeleter<T>());
    if (GTIFKeyGet(m_rawgtif, key, buffer.get(), 0, count) != count) {
        throw std::runtime_error("GeoTIFF key read error.");
    }
    return std::make_tuple(count, buffer);
};

template <typename T>
bool GeoTiff::GetKeySingle(geokey_t key, T *output) const {
    auto result = GetKey<T>(key);
    if (std::get<0>(result) != 1) {
        return false;
    }
    *output = *std::get<1>(result).get();
    return true;
}

bool GeoTiff::HasKey(geokey_t key) const {
    return 0 != GTIFKeyInfo(m_rawgtif, key, NULL, NULL);
}

bool GeoTiff::PixelToPCS(double *x, double *y) const {
    if (m_type == RasterMap::TYPE_IMAGE)
        return false;

    if (m_ntiepoints > 6 && m_npixscale == 0) {
        // Interpolate between multiple tiepoints
        if (m_ntiepoints != 4*6) {
            // Currently, we only support 4 tiepoints.
            return false;
        };

        unsigned int w = GetWidth();
        unsigned int h = GetHeight();
        std::unique_ptr<MapPixelDelta> p00, p10, p01, p11;

        for (unsigned int i=0; i < m_ntiepoints; i += 6) {
            if (m_tiepoints[i+0] == 0 && m_tiepoints[i+1] == 0)
                p00.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == w && m_tiepoints[i+1] == 0)
                p10.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == 0 && m_tiepoints[i+1] == h)
                p01.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == w && m_tiepoints[i+1] == h)
                p11.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else {
                // For now, we require tie points to be at the image corners.
                return false;
            }
        }
        if (!p00 || !p10 || !p01 || !p11) {
            return false;
        }
        auto interpolator = BilinearInterpolator(*p00, *p10, *p11, *p01);
        auto res = interpolator.forward(UnitSquareCoord(*x/w, *y/h));
        *x = res.x;
        *y = res.y;
    }
    else if (m_ntransform == 16) {
        // Use matrix for transformation
        double x_in = *x, y_in = *y;
        *x = x_in * m_transform[0] + y_in * m_transform[1] + m_transform[3];
        *y = x_in * m_transform[4] + y_in * m_transform[5] + m_transform[7];
    }
    else if (m_npixscale >= 3 && m_ntiepoints >= 6) {
        // Use one tiepoint + pixscale
        *x = (*x - m_tiepoints[0]) * m_pixscale[0] + m_tiepoints[3];
        *y = (*y - m_tiepoints[1]) * (-1 * m_pixscale[1]) + m_tiepoints[4];
    }
    else {
        throw std::runtime_error("Couldn't find GeoTIFF coordinates.");
    }
    return true;
}

bool GeoTiff::PCSToPixel(double *x, double *y) const {
    if (m_type == RasterMap::TYPE_IMAGE)
        return false;

    if (m_ntiepoints > 6 && m_npixscale == 0) {
        // Interpolate between multiple tiepoints
        if (m_ntiepoints != 4*6) {
            // Currently, we only support 4 tiepoints.
            return false;
        }

        unsigned int w = GetWidth();
        unsigned int h = GetHeight();
        std::unique_ptr<MapPixelDelta> p00, p10, p01, p11;

        for (unsigned int i=0; i < m_ntiepoints; i += 6) {
            if (m_tiepoints[i+0] == 0 && m_tiepoints[i+1] == 0)
                p00.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == w && m_tiepoints[i+1] == 0)
                p10.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == 0 && m_tiepoints[i+1] == h)
                p01.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else if (m_tiepoints[i+0] == w && m_tiepoints[i+1] == h)
                p11.reset(new MapPixelDelta(m_tiepoints[i+3], m_tiepoints[i+4]));
            else {
                // For now, we require tie points to be at the image corners.
                return false;
            }
        }
        if (!p00 || !p10 || !p01 || !p11) {
            return false;
        }
        auto interpolator = BilinearInterpolator(*p00, *p10, *p11, *p01);
        auto res = interpolator.inverse(MapPixelDelta(*x, *y));
        *x = res.x * w;
        *y = res.y * h;
    }
    else if (m_ntransform == 16) {
        // Use inverse matrix for transformation.
        const double *mat = m_transform;
        double x_in = *x, y_in = *y;
        double denom = (mat[0] * mat[5] - mat[1] * mat[4]);
        *x = (+(x_in - mat[3]) * mat[5] - (y_in - mat[7]) * mat[1]) / denom;
        *y = (-(x_in - mat[3]) * mat[4] + (y_in - mat[7]) * mat[0]) / denom;
    }
    else if (m_npixscale >= 3 && m_ntiepoints >= 6) {
        // Use one tiepoint + pixscale
        *x = (*x - m_tiepoints[3]) / m_pixscale[0] + m_tiepoints[0];
        *y = (*y - m_tiepoints[4]) / (-1 * m_pixscale[1]) + m_tiepoints[1];
    }
    else {
        throw std::runtime_error("Couldn't find GeoTIFF coordinates.");
    }
    return true;
}

bool GeoTiff::CheckDHMValid() const {
    short int vert_units;
    if (!GetKeySingle<short int>(VerticalUnitsGeoKey, &vert_units))
        return false;

    // Other vertical formats not supported at the moment
    return vert_units == Linear_Meter;
}

// If we have a DHM map, the pixel values typically represent height in m
// The default TIFFRGBAImageGet turns this (typically 16 bit) value into
// a grayscale rgb image (thus truncating the lower 8 bits).
// Prevent this and simply store the 16 bit value within the 32 bit RGBA.
void GeoTiff::Hook_TIFFRGBAImageGet(TIFFRGBAImage &img) const {
    // Only hook DHM maps
    if (GetType() != RasterMap::TYPE_DHM)
        return;

    if (GetBitsPerSample() != 16) {
        assert(false);  // not implemented
    }
    if (!img.isContig) {
        assert(false);  // not implemented
    }
    if (img.photometric != PHOTOMETRIC_MINISWHITE &&
        img.photometric != PHOTOMETRIC_MINISBLACK)
    {
        assert(false);  // not implemented
    }
    img.put.contig = put16bitbw_DHM;
}

TiffMap::TiffMap(const wchar_t *fname)
    : m_geotiff(new GeoTiff(fname)), m_proj(m_geotiff->GetProj4String()),
      m_description()
{}

GeoDrawable::DrawableType TiffMap::GetType() const {
    return m_geotiff->GetType();
}
unsigned int TiffMap::GetWidth() const { return m_geotiff->GetWidth(); };
unsigned int TiffMap::GetHeight() const { return m_geotiff->GetHeight(); };
MapPixelDeltaInt TiffMap::GetSize() const {
    return MapPixelDeltaInt(m_geotiff->GetWidth(), m_geotiff->GetHeight());
}

PixelBuf TiffMap::GetRegion(
                      const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const
{
    auto fixed_bounds_pb = GetRegion_BoundsHelper(*this, pos, size);
    if (fixed_bounds_pb.GetData())
        return fixed_bounds_pb;

    return m_geotiff->GetRegion(pos, size);
}

bool TiffMap::PixelToPCS(double *x, double *y) const
    { return m_geotiff->PixelToPCS(x, y); }
bool TiffMap::PCSToPixel(double *x, double *y) const
    { return m_geotiff->PCSToPixel(x, y); }
const std::wstring &TiffMap::GetFname() const
    { return m_geotiff->GetFilename(); }
const std::wstring &TiffMap::GetTitle() const
    { return m_geotiff->GetTitle(); }
const std::wstring &TiffMap::GetDescription() const
    { return m_description; }
Projection TiffMap::GetProj() const
    { return m_proj; }

bool TiffMap::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
    double x = pos.x;
    double y = pos.y;
    if (!PixelToPCS(&x, &y))
        return false;

    if (m_geotiff->GetModel() == ModelTypeProjected) {
        if (!GetProj().PCSToLatLong(x, y)) {
            return false;
        }
    }
    *result = LatLon(y, x);
    return true;
}

bool TiffMap::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
    double x = pos.lon;
    double y = pos.lat;
    if (m_geotiff->GetModel() == ModelTypeProjected) {
        if (!GetProj().LatLongToPCS(x, y)) {
            return false;
        }
    }
    if (!PCSToPixel(&x, &y))
        return false;

    *result = MapPixelCoord(x, y);
    return true;
}

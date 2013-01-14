#include <string>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tiff.h"
#include "libxtiff/xtiffio.h"
#include "geotiffio.h"
#include "geovalues.h"
#include "geo_normalize.h"

#include "rastermap.h"
#include "geotiff_impl.h"
#include "map_geotiff.h"
#include "projection.h"

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
    : m_fname(fname), m_tiffhandle(fname),
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
};

std::shared_ptr<unsigned int> Tiff::GetRegion(int x, int y,
                              unsigned int width, unsigned int height) const
{

    unsigned int ux = (x >= 0) ? x : 0;
    unsigned int uy = (y >= 0) ? y : 0;
    if (x + width < 0 || y + height < 0 || ux > m_width || uy > m_height) {
        // Return zero-initialized memory block (notice the parentheses)
        return std::shared_ptr<unsigned int>(new unsigned int[width * height](),
                                             ArrayDeleter<unsigned int>());
    }

    TIFFRGBAImage img;
    char emsg[1024] = "";
    if (!TIFFRGBAImageOK(m_rawtiff, emsg) || 
        !TIFFRGBAImageBegin(&img, m_rawtiff, 0, emsg)) {
        throw std::runtime_error("TIFF RGBA access not possible.");
    }

    img.col_offset = x;
    img.row_offset = y;

    // Give derived classes a chance to influence TIFFRGBAGetImage
    Hook_TIFFRGBAImageGet(img);

    std::shared_ptr<unsigned int> raster(new unsigned int[width * height],
                                         ArrayDeleter<unsigned int>());
    int ok = TIFFRGBAImageGet(&img, raster.get(), width, height);
    TIFFRGBAImageEnd(&img);

    if (!ok) {
        throw std::runtime_error("Loading TIFF data failed.");
    }
    return raster;
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
    if (!CheckVersion()) {
        throw std::runtime_error("GeoTIFF version not supported.");
    }
    LoadCoordinates();
};

bool GeoTiff::CheckVersion() const {
    enum GTIFVersion { VERSION = 0, REV_MAJOR, REV_MINOR };
    int versions[3];
    GTIFDirectoryInfo(m_rawgtif, versions, NULL);
    return versions[VERSION] <= GvCurrentVersion &&
           versions[REV_MAJOR] <= GvCurrentRevision;
}

void GeoTiff::LoadCoordinates() {
    geocode_t model = GetKeySingle<geocode_t>(GTModelTypeGeoKey);
    assert(model == ModelTypeProjected); // The rest is not implemented

    GTIFDefn    defn;
    if(GTIFGetDefn(m_rawgtif, &defn))
    {
        //printf( "\n" );
        //GTIFPrintDefn(&defn, stdout);

        std::shared_ptr<char> proj_str(GTIFGetProj4Defn(&defn),
                                       FreeDeleter<char>());
        if (!proj_str) {
            throw std::runtime_error("Failed to get projection definition.");
        }
        m_proj = proj_str.get();
    }

    std::tie(m_ntiepoints, m_tiepoints) = GetField<double>(TIFFTAG_GEOTIEPOINTS);
    std::tie(m_npixscale, m_pixscale) = GetField<double>(TIFFTAG_GEOPIXELSCALE);
    std::tie(m_ntransform, m_transform) = GetField<double>(TIFFTAG_GEOTRANSMATRIX);

    if (HasKey(VerticalUnitsGeoKey)) {
        // Vertical coordinate system defined -> DHM
        m_type = RasterMap::TYPE_DHM;
    } else {
        m_type = RasterMap::TYPE_MAP;
    }
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
T GeoTiff::GetKeySingle(geokey_t key) const {
    auto result = GetKey<T>(key);
    if (std::get<0>(result) != 1) {
        throw std::runtime_error("Couldn't read GeoTIFF key.");
    }
    return *std::get<1>(result).get();
}

bool GeoTiff::HasKey(geokey_t key) const {
    return 0 != GTIFKeyInfo(m_rawgtif, key, NULL, NULL);
}

void GeoTiff::PixelToPCS(double *x, double *y) const {

    if (m_ntiepoints > 6 && m_npixscale == 0) {
        // Interpolate between multiple tiepoints
        assert(false);  // Not implemented
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
}

void GeoTiff::PCSToPixel(double *x, double *y) const {

    if (m_ntiepoints > 6 && m_npixscale == 0) {
        // Interpolate between multiple tiepoints
        assert(false);  // Not implemented
    }
    else if (m_ntransform == 16) {
        // Use matrix for transformation
        assert(false);  // Not implemented
    }
    else if (m_npixscale >= 3 && m_ntiepoints >= 6) {
        // Use one tiepoint + pixscale
        *x = (*x - m_tiepoints[3]) / m_pixscale[0] + m_tiepoints[0];
        *y = (*y - m_tiepoints[4]) / (-1 * m_pixscale[1]) + m_tiepoints[1];
    }
    else {
        throw std::runtime_error("Couldn't find GeoTIFF coordinates.");
    }
}

bool GeoTiff::CheckDHMValid() const {
    // Other vertical formats not supported at the moment
    return GetKeySingle<short int>(VerticalUnitsGeoKey) == Linear_Meter;
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
    : m_geotiff(new GeoTiff(fname)), m_proj(NULL)
{
    m_proj.reset(new Projection(m_geotiff->GetProj4String()));
    if (!m_proj) {
        throw std::runtime_error("Unable to initialize projection.");
    }
    if (m_geotiff->GetType() == TYPE_DHM) {

    }
};


RasterMap::RasterMapType TiffMap::GetType() const { return m_geotiff->GetType(); };
unsigned int TiffMap::GetWidth() const { return m_geotiff->GetWidth(); };
unsigned int TiffMap::GetHeight() const { return m_geotiff->GetHeight(); };
std::shared_ptr<unsigned int> TiffMap::GetRegion(int x, int y,
    unsigned int width, unsigned int height) const
    { return m_geotiff->GetRegion(x, y, width, height); };
void TiffMap::PixelToPCS(double *x, double *y) const
    { return m_geotiff->PixelToPCS(x, y); }
void TiffMap::PCSToPixel(double *x, double *y) const
    { return m_geotiff->PCSToPixel(x, y); }
const std::wstring &TiffMap::GetFname() const
    { return m_geotiff->GetFilename(); }
const class Projection &TiffMap::GetProj() const
    { return *m_proj; }

void TiffMap::PixelToLatLong(double *x, double *y) const {
    PixelToPCS(x, y);
    GetProj().PCSToLatLong(*x, *y);
}

void TiffMap::LatLongToPixel(double *x, double *y) const {
    GetProj().LatLongToPCS(*x, *y);
    PCSToPixel(x, y);
}

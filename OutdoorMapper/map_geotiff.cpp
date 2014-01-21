#include <string>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <algorithm>

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

MapRegion
Tiff::GetRegion(const MapPixelCoordInt &pos,
                const MapPixelDeltaInt &size) const
{
    MapPixelCoordInt endpos = pos + size;
    if (endpos.x < 0 || endpos.y < 0 ||
        pos.x > static_cast<int>(m_width) ||
        pos.y > static_cast<int>(m_height))
    {
        // Return zero-initialized memory block (notice the parentheses)
        return MapRegion(std::shared_ptr<unsigned int>(new unsigned int[size.x*size.y](),
                                             ArrayDeleter<unsigned int>()),
                         size.x, size.y);
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

    std::shared_ptr<unsigned int> raster(new unsigned int[size.x * size.y],
                                         ArrayDeleter<unsigned int>());
    int ok = TIFFRGBAImageGet(&img, raster.get(), size.x, size.y);
    TIFFRGBAImageEnd(&img);

    if (!ok) {
        throw std::runtime_error("Loading TIFF data failed.");
    }
    return MapRegion(raster, size.x, size.y);
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

    if (HasKey(VerticalUnitsGeoKey)) {
        // Vertical coordinate system defined -> DHM
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
    return true;
}

bool GeoTiff::PCSToPixel(double *x, double *y) const {
    if (m_type == RasterMap::TYPE_IMAGE)
        return false;

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
    : m_geotiff(new GeoTiff(fname)), m_proj(m_geotiff->GetProj4String())
{}

RasterMap::RasterMapType TiffMap::GetType() const {
    return m_geotiff->GetType();
}
unsigned int TiffMap::GetWidth() const { return m_geotiff->GetWidth(); };
unsigned int TiffMap::GetHeight() const { return m_geotiff->GetHeight(); };
MapPixelDeltaInt TiffMap::GetSize() const {
    return MapPixelDeltaInt(m_geotiff->GetWidth(), m_geotiff->GetHeight());
}

MapRegion TiffMap::GetRegion(
                      const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const
    { return m_geotiff->GetRegion(pos, size); };
bool TiffMap::PixelToPCS(double *x, double *y) const
    { return m_geotiff->PixelToPCS(x, y); }
bool TiffMap::PCSToPixel(double *x, double *y) const
    { return m_geotiff->PCSToPixel(x, y); }
const std::wstring &TiffMap::GetFname() const
    { return m_geotiff->GetFilename(); }
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

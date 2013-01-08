#ifndef ODM__GEOTIFF_IMPL_H
#define ODM__GEOTIFF_IMPL_H

#include <memory>
#include <tuple>

#include "libxtiff/xtiffio.h"
#include "geotiffio.h"

#include "util.h"

class TiffHandle {
    public:
        explicit TiffHandle(const std::wstring &fname)
            : m_tiff(XTIFFOpenW(fname.c_str(), "r"))
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
        unsigned int *GetRegion(int x, int y,
                                unsigned int width, unsigned int height) const;

        template <typename T>
        std::tuple<unsigned int, const T*>
        GetField(ttag_t field) const;


        const std::wstring &GetFilename() const { return m_fname; };
    protected:
        const std::wstring m_fname;
        TiffHandle m_tiffhandle;
        TIFF *m_rawtiff;

        virtual void Hook_TIFFRGBAImageGet(TIFFRGBAImage &img) const {};
    private:
        unsigned int m_width, m_height;
        unsigned short int m_bitspersample, m_samplesperpixel;
};

class GeoTiff : public Tiff {
    public:
        explicit GeoTiff(const std::wstring &fname);
        virtual ~GeoTiff() { };

        bool CheckVersion() const;
        void LoadCoordinates();

        void PixelToPCS(double *x, double *y) const;
        void PCSToPixel(double *x, double *y) const;
        const std::string &GetProj4String() const { return m_proj; };
        RasterMap::RasterMapType GetType() const { return m_type; };

        template <typename T>
        std::tuple<unsigned int, std::shared_ptr<T>>
        GetKey(geokey_t key) const;

        template <typename T>
        T GetKeySingle(geokey_t key) const;

        bool HasKey(geokey_t) const;

    protected:
        GeoTiffHandle m_gtifhandle;
        GTIF *m_rawgtif;

        virtual void Hook_TIFFRGBAImageGet(TIFFRGBAImage &img) const;
    private:
        bool CheckDHMValid() const;

        const double *m_tiepoints;
        const double *m_pixscale;
        const double *m_transform;
        unsigned short int m_ntiepoints, m_npixscale, m_ntransform;

        std::string m_proj;
        RasterMap::RasterMapType m_type;
};

#endif

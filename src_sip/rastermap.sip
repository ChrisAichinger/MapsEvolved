typedef unsigned int size_t;

class MapRegion
{
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        //MapRegion(UIntShPtr &data, int width, int height);
        //inline std::shared_ptr<unsigned int> &GetData() { return m_data; }
        //inline unsigned int * GetRawData() { return m_data.get(); }
        unsigned int GetWidth() const;
        unsigned int GetHeight() const;
        unsigned int GetPixel(int x, int y) const;
        PyObject *GetData();
        %MethodCode
            unsigned int size = sipCpp->GetWidth() * sipCpp->GetHeight() *
                                 sizeof(unsigned int);
            sipRes = sipConvertFromConstVoidPtrAndSize(sipCpp->GetRawData(),
                                                       size);
        %End
};


class GeoPixels /Abstract/ {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        virtual ~GeoPixels();
        virtual bool PixelToLatLon(
                const MapPixelCoord &pos, LatLon *result /Out/) const = 0;
        virtual bool LatLonToPixel(
                const LatLon &pos, MapPixelCoord *result /Out/) const = 0;
};


class GeoDrawable : public GeoPixels /Abstract/ {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        enum DrawableType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT_MAP,
            TYPE_STEEPNESS_MAP,
            TYPE_LEGEND,
            TYPE_OVERVIEW,
            TYPE_IMAGE,
            TYPE_GPSTRACK,
            TYPE_GRIDLINES,
            TYPE_POI_DB,
            TYPE_ERROR,
        };
        virtual ~GeoDrawable();

        virtual DrawableType GetType() const = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual MapPixelDeltaInt GetSize() const = 0;
        MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual Projection GetProj() const = 0;

        virtual const std::wstring &GetFname() const = 0;

        virtual bool IsViewable() const;
        virtual bool SupportsDirectDrawing() const;
        virtual MapRegion
        GetRegionDirect(const MapPixelDeltaInt &output_size,
                        const GeoPixels &base,
                        const MapPixelCoord &base_tl,
                        const MapPixelCoord &base_br) const;
};

class GeoDrawableShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
#include "rastermap.h"
typedef std::shared_ptr<GeoDrawable> GeoDrawableShPtr;
%End
    public:
        GeoDrawableShPtr(GeoDrawable* ref /Transfer/);
        GeoDrawableShPtr(const GeoDrawableShPtr& ref);
        GeoDrawable *get();
};

class RasterMap : public GeoDrawable /Abstract/ {
    public:
        virtual ~RasterMap();
};

class RasterMapShPtr /Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
#include "rastermap.h"
typedef std::shared_ptr<RasterMap> RasterMapShPtr;
%End
    public:
        // Take care to make RasterMapShPtr default- and copy-constructable
        // from SIP's view. Otherwise it won't generate an AssignmentHelper,
        // and we get access violations when trying to move RasterMapShPtr
        // objects from Python back to C++.
        RasterMapShPtr();
        RasterMapShPtr(RasterMap* ref /Transfer/);
        RasterMapShPtr(const RasterMapShPtr& ref);
        RasterMap *get();
};


struct TerrainInfo {
%TypeHeaderCode
#include "rastermap.h"
%End
    double height_m;
    double slope_face_deg;
    double steepness_deg;
};

RasterMapShPtr LoadMap(const std::wstring &fname);
std::vector<RasterMapShPtr> AlternateMapViews(const RasterMapShPtr &map);

class HeightFinder {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        virtual bool CalcTerrain(const LatLon &pos, TerrainInfo *result /Out/);
        RasterMapShPtr GetActiveMap() const;
    protected:
        virtual bool LatLongWithinActiveDHM(const LatLon &pos) const;
        virtual RasterMapShPtr FindBestMap(
                      const LatLon &pos,
                      GeoDrawable::DrawableType type) const;
};

bool GetMapDistance(const RasterMapShPtr &map, const MapPixelCoord &pos,
                    double dx, double dy, double *distance /Out/);
bool MetersPerPixel(const RasterMapShPtr &map, const MapPixelCoord &pos,
                    double *mpp /Out/);
bool MetersPerPixel(const RasterMapShPtr &map, const MapPixelCoordInt &pos,
                    double *mpp /Out/);
bool GetMapDistance(const GeoDrawableShPtr &map, const MapPixelCoord &pos,
                    double dx, double dy, double *distance /Out/);
bool MetersPerPixel(const GeoDrawableShPtr &map, const MapPixelCoord &pos,
                    double *mpp /Out/);
bool MetersPerPixel(const GeoDrawableShPtr &map, const MapPixelCoordInt &pos,
                    double *mpp /Out/);


class GPSSegment : public GeoDrawable {
%TypeHeaderCode
#include "gpstracks.h"
%End
    public:
        GPSSegment(const std::wstring &fname,
                   const std::vector<LatLon> &points);
        virtual const std::vector<LatLon> &GetPoints() const;
};

class GPSSegmentShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
#include "gpstracks.h"
typedef std::shared_ptr<GPSSegment> GPSSegmentShPtr;
%End
    public:
        GPSSegmentShPtr(const std::wstring &fname,
                        const std::vector<LatLon> &points);
        %MethodCode
            sipCpp = new std::shared_ptr<GPSSegment>(new GPSSegment(*a0, *a1));
        %End
        GPSSegment *get();
};

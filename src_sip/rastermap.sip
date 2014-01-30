typedef unsigned int size_t;

class MapRegion /NoDefaultCtors/
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
        unsigned int GetPixel(int x, int y);
};


class GeoDrawable /Abstract/ {
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

        virtual bool PixelToPCS(double *x /In,Out/,
                                double *y /In,Out/) const = 0;
        virtual bool PCSToPixel(double *x /In,Out/,
                                double *y /In,Out/) const = 0;
        virtual Projection GetProj() const = 0;
        virtual bool PixelToLatLon(
                const MapPixelCoord &pos, LatLon *result /Out/) const = 0;
        virtual bool LatLonToPixel(
                const LatLon &pos, MapPixelCoord *result /Out/) const = 0;

        virtual const std::wstring &GetFname() const = 0;

        virtual bool IsViewable() const;
};

class GeoDrawableShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
#include "rastermap.h"
typedef std::shared_ptr<GeoDrawable> GeoDrawableShPtr;
%End
    public:
        GeoDrawable *get();
};

class RasterMap : public GeoDrawable /Abstract/ {
    public:
        virtual ~RasterMap();
};

class RasterMapShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
#include "rastermap.h"
typedef std::shared_ptr<RasterMap> RasterMapShPtr;
%End
    public:
        RasterMap *get();
};

class PersistentStoreUniqPtr
/NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/
{
%TypeHeaderCode
#include <memory>
#include <stdio.h>
%End
    public:
        const PersistentStore *get();
};

PersistentStoreUniqPtr CreatePersistentStore();

class RasterMapCollection {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        RasterMapCollection();
        void AddMap(RasterMapShPtr map);
        void DeleteMap(unsigned int index);
        size_t Size() const;
        RasterMapShPtr Get(size_t i) const;
        const std::vector<const RasterMapShPtr>
            GetAlternateRepresentations(size_t i) const;
        bool IsToplevelMap(const RasterMapShPtr &map) const;

        bool StoreTo(const PersistentStoreUniqPtr &store /In/) const;
        bool RetrieveFrom(const PersistentStoreUniqPtr &store /In/);
    private:
        RasterMapCollection(const RasterMapCollection&);
        void operator=(const RasterMapCollection&);
};

void LoadMap(RasterMapCollection &maps, const std::wstring &fname);

struct TerrainInfo {
%TypeHeaderCode
#include "rastermap.h"
%End
    double height_m;
    double slope_face_deg;
    double steepness_deg;
};

class HeightFinder {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        explicit HeightFinder(const RasterMapCollection &maps);
        bool CalcTerrain(const LatLon &pos, TerrainInfo *result /Out/);
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

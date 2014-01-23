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


class RasterMap /Abstract/ {
%TypeHeaderCode
#include "rastermap.h"
%End
    public:
        enum RasterMapType {
            TYPE_MAP = 1,
            TYPE_DHM,
            TYPE_GRADIENT,
            TYPE_STEEPNESS,
            TYPE_LEGEND,
            TYPE_OVERVIEW,
            TYPE_ERROR,
            TYPE_IMAGE,
        };
        virtual ~RasterMap();

        virtual RasterMapType GetType() const = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual MapPixelDeltaInt GetSize() const = 0;
        MapRegion
            GetRegion(const MapPixelCoordInt &pos,
                      const MapPixelDeltaInt &size) const;

        virtual bool PixelToPCS(double *x, double *y) const = 0;
        virtual bool PCSToPixel(double *x, double *y) const = 0;
        virtual Projection GetProj() const = 0;
        virtual bool
        PixelToLatLon(const MapPixelCoord &pos, LatLon *result /Out/) const = 0;
        virtual bool
        LatLonToPixel(const LatLon &pos, MapPixelCoord *result /Out/) const = 0;

        virtual const std::wstring &GetFname() const = 0;

        virtual bool IsViewable() const;
};

class RasterMapShPtrConst /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
%End
    public:
        const RasterMap *get();
};

class RasterMapShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
%End
    public:
        RasterMap *get();
};

class PersistentStoreUniqPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
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
        const RasterMap &Get(size_t i) const;
        RasterMapShPtrConst GetSharedPtr(size_t i) const;
        /*
        const std::vector<const RasterMapShPtrConst>
            GetAlternateRepresentations(size_t i) const;
            */
        bool IsToplevelMap(const RasterMapShPtrConst &map) const;

        //bool StoreTo(PersistentStore *store) const;
        //bool RetrieveFrom(PersistentStore *store);
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
    private:
};


bool GetMapDistance(const RasterMap &map, const MapPixelCoord &pos,
                    double dx, double dy, double *distance /Out/);
bool MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos,
                    double *mpp /Out/);
bool MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos,
                    double *mpp /Out/);

/*
%ModuleHeaderCode
#define CALL_FUNC_NOT_NULL(sipIsErr, type, func, ...)                    \
    do {                                                                 \
        type _CALL_FUNC_NOT_NULL_VAR;                                    \
        if (!func(__VA_ARGS__, &_CALL_FUNC_NOT_NULL_VAR)) {              \
            PyErr_SetString(PyExc_RuntimeError, "Failed " #func "()");   \
            sipIsErr = 1;                                                \
        }                                                                \
        sipRes = _CALL_FUNC_NOT_NULL_VAR;                                \
    } while(0)
%End


double GetMapDistance(const RasterMap &map, const MapPixelCoord &pos, double dx, double dy);
%MethodCode
    CALL_FUNC_NOT_NULL(sipIsErr, double, GetMapDistance, *a0, *a1, a2, a3);
%End

double MetersPerPixel(const RasterMap &map, const MapPixelCoord &pos);
%MethodCode
    CALL_FUNC_NOT_NULL(sipIsErr, double, MetersPerPixel, *a0, *a1);
%End
double MetersPerPixel(const RasterMap &map, const MapPixelCoordInt &pos);
%MethodCode
    CALL_FUNC_NOT_NULL(sipIsErr, double, MetersPerPixel, *a0, *a1);
%End
*/


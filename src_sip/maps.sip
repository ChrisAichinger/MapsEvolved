%Module maplib_sip

%ModuleHeaderCode
#include <smartptr_proxy.h>
%End

%InitialisationCode
smartptr_proxy_init("maplib_sip", sipAPI_maplib_sip);
%End

%PostInitialisationCode
smartptr_proxy_post_init(sipModuleDict);
%End


%Include std_types.sip
%Include coordinates.sip
%Include rastermap.sip


class Projection {
    public:
%TypeHeaderCode
#include <string>
#include "projection.h"
#include "util.h"
%End
        explicit Projection(const std::string &proj_str);

        bool PCSToLatLong(double &x, double &y) const;
        bool LatLongToPCS(double &x, double &y) const;
        const std::string &GetProjString() const;

        bool CalcDistance(double lat1, double long1,
                          double lat2, double long2,
                          double *distance) const;

        bool IsValid() const;
};


class PersistentStore /Abstract/ {
    public:
        virtual ~PersistentStore();
        virtual bool OpenRead() = 0;
        virtual bool OpenWrite() = 0;
        virtual bool IsOpen() = 0;
        virtual void Close() = 0;

        virtual std::vector<std::wstring> *GetStringList(const std::wstring &keyvalue) = 0;
        %MethodCode
            std::unique_ptr< std::vector<std::wstring> > strings(new std::vector<std::wstring>);
            bool res = sipCpp->GetStringList(*a0, strings.get());
            if (!res) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to GetStringList()");
                sipIsErr = 1;
            } else {
                sipRes = strings.release();
            }
        %End
        virtual void SetStringList(const std::wstring &keyvalue,
                                   const std::vector<std::wstring> &strings) = 0;
        %MethodCode
            bool res = sipCpp->SetStringList(*a0, *a1);
            if (!res) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to SetStringList()");
                sipIsErr = 1;
            }
        %End

        virtual unsigned int GetUInt(const std::wstring &keyvalue) = 0;
        %MethodCode
            unsigned long int value;
            bool res = sipCpp->GetUInt(*a0, &value);
            if (!res) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to GetUInt()");
                sipIsErr = 1;
            } else {
                sipRes = value;
            }
        %End

        virtual void SetUInt(const std::wstring &keyvalue,
                             unsigned int value) = 0;
        %MethodCode
            bool res = sipCpp->SetUInt(*a0, a1);
            if (!res) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to SetUInt()");
                sipIsErr = 1;
            }
        %End
};


class Display /Abstract/ {
%TypeHeaderCode
#include "disp_ogl.h"
%End
    public:
        virtual unsigned int GetDisplayWidth() const = 0;
        virtual unsigned int GetDisplayHeight() const = 0;
        virtual DisplayDelta GetDisplaySize() const = 0;

        //virtual void Render(class std::list<class DisplayOrder> &orders) = 0;
        virtual void Resize(unsigned int width, unsigned int height) = 0;
        virtual void ForceRepaint() = 0;
};

class DispOpenGL : public Display /NoDefaultCtors/ {
%TypeHeaderCode
#include "disp_ogl.h"
%End
    public:
        //explicit DispOpenGL(const std::shared_ptr<OGLContext> &ogl_context);

        virtual unsigned int GetDisplayWidth() const;
        virtual unsigned int GetDisplayHeight() const;
        virtual DisplayDelta GetDisplaySize() const;

        //virtual void Render(class std::list<class DisplayOrder> &orders);
        virtual void Resize(unsigned int width, unsigned int height);
        virtual void ForceRepaint();
};

DisplayCoordCentered CenteredCoordFromDisplay(const DisplayCoord& dc,
                                              const Display& disp);
DisplayCoord DisplayCoordFromCentered(const DisplayCoordCentered& dc,
                                      const Display& disp);


class DispOpenGLShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
typedef std::shared_ptr<class DispOpenGL> DispOpenGLShPtr;
%End
    public:
        DispOpenGL *get();
};

%ModuleCode
#include <winwrap.h>
%End
DispOpenGLShPtr CreateOGLDisplay(size_t handle);
%MethodCode
    std::shared_ptr<DevContext> dev_ctx(new DevContext((HWND)a0));
    std::shared_ptr<OGLContext> ogl_ctx(new OGLContext(dev_ctx));
    sipRes = new std::shared_ptr<DispOpenGL>(new DispOpenGL(ogl_ctx));
%End


class MapDisplayManager /NoDefaultCtors/ {
%TypeHeaderCode
#include "rastermap.h"
#include "mapdisplay.h"
%End
    public:
        MapDisplayManager(const DispOpenGLShPtr &display,
                          const RasterMapShPtr &initial_map);

        RasterMapShPtr GetBaseMap() const;
        double GetZoom() const;
        double GetCenterX() const;
        double GetCenterY() const;
        const BaseMapCoord &GetCenter() const;

        void ChangeMap(const RasterMapShPtr &new_map,
                       bool try_preserve_pos=true);
        void AddOverlayMap(const RasterMapShPtr new_map);

        void Resize(unsigned int width, unsigned int height);
        void StepZoom(double steps);
        // The map location under the mouse is held constant
        void StepZoom(double steps, const DisplayCoord &mouse_pos);
        void SetZoomOneToOne();
        void DragMap(const DisplayDelta &delta);
        void CenterToDisplayCoord(const DisplayCoord &center);
        void Paint();

        BaseMapCoord BaseCoordFromDisplay(const DisplayCoord &disp) const;
        BaseMapCoord
            BaseCoordFromDisplay(const DisplayCoordCentered &disp) const;

        BaseMapDelta BaseDeltaFromDisplay(const DisplayDelta &disp) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromBase(const BaseMapCoord &mpc) const;

        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoord &mpc,
                                         const RasterMapShPtr &map) const;
        DisplayCoordCentered
        DisplayCoordCenteredFromMapPixel(const MapPixelCoordInt &mpc,
                                         const RasterMapShPtr &map) const;
};

std::wstring CompassPointFromDirection(double degrees);

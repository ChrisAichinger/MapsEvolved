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
%Include map_definitions.sip
%Include map_implementation_gvg.sip
%Include exceptions.sip


class Projection {
%TypeHeaderCode
#include <string>
#include "projection.h"
#include "util.h"
%End
    public:
        explicit Projection(const char *proj_str /Encoding="ASCII"/);

        bool PCSToLatLong(double &x, double &y) const;
        bool LatLongToPCS(double &x, double &y) const;
        const std::string &GetProjString() const;

        bool CalcDistance(double lat1, double long1,
                          double lat2, double long2,
                          double *distance) const;

        bool IsValid() const;
};


class Display /Abstract/ {
%TypeHeaderCode
#include "display.h"
%End
    public:
        virtual unsigned int GetDisplayWidth() const = 0;
        virtual unsigned int GetDisplayHeight() const = 0;
        virtual DisplayDeltaInt GetDisplaySize() const = 0;
        virtual void SetDisplaySize(const DisplayDeltaInt &new_size) = 0;

        //virtual void Render(class std::list<class DisplayOrder> &orders) = 0;
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
        virtual void SetDisplaySize(const DisplayDeltaInt &new_size);

        //virtual void Render(class std::list<class DisplayOrder> &orders);
        virtual void ForceRepaint();
};

DisplayCoordCentered CenteredCoordFromDisplay(const DisplayCoord& dc,
                                              const Display& disp);
DisplayCoord DisplayCoordFromCentered(const DisplayCoordCentered& dc,
                                      const Display& disp);


class DisplayShPtr /NoDefaultCtors,Supertype=maplib_sip.SmartptrProxy/ {
%TypeHeaderCode
#include <memory>
typedef std::shared_ptr<class Display> DisplayShPtr;
%End
    public:
        Display *get();
};

%ModuleCode
#include "winwrap.h"
#include "disp_ogl.h"
%End
DisplayShPtr CreateOGLDisplay(size_t handle);
%MethodCode
    std::shared_ptr<DevContext> dev_ctx(new DevContext((HWND)a0));
    std::shared_ptr<OGLContext> ogl_ctx(new OGLContext(dev_ctx));
    sipRes = new std::shared_ptr<Display>(new DispOpenGL(ogl_ctx));
%End


class OverlaySpec {
%TypeHeaderCode
#include "rastermap.h"
#include "mapdisplay.h"
%End
    public:
        OverlaySpec();
        OverlaySpec(const GeoDrawableShPtr &map, bool enabled = true,
                    float opacity = 0.5f);
        OverlaySpec(const RasterMapShPtr &map, bool enabled = true,
                    float opacity = 0.5f);
        GeoDrawableShPtr GetMap() const;
        bool GetEnabled() const;
        float GetTransparency() const;
        void SetMap(const GeoDrawableShPtr &map);
        void SetEnabled(bool enabled);
        void SetTransparency(float transparency);
        %Property(name="Map", get=GetMap, set=SetMap)
        %Property(name="Enabled", get=GetEnabled, set=SetEnabled)
        %Property(name="Transparency", get=GetTransparency, set=SetTransparency)
};

typedef std::vector<OverlaySpec> OverlayList;


enum ODMPixelFormat {
    ODM_PIX_INVALID = 0,
    ODM_PIX_RGBA4,
    ODM_PIX_RGBX4,
};

class MapDisplayManager {
%TypeHeaderCode
#include "rastermap.h"
#include "mapdisplay.h"
%End
    public:
        MapDisplayManager(const GeoDrawableShPtr &initial_map,
                          const DisplayDeltaInt &display_size);
        MapDisplayManager(const RasterMapShPtr &initial_map,
                          const DisplayDeltaInt &display_size);


        double GetZoom() const;
        void StepZoom(double steps);
        void StepZoom(double steps, const DisplayCoord &mouse_pos);
        void SetZoomOneToOne();

        const BaseMapCoord &GetCenter() const;
        void SetCenter(const BaseMapCoord &center);
        void SetCenter(const LatLon &center);
        void SetCenter(const DisplayCoord &center);
        void MoveCenter(const DisplayDelta &delta);

        GeoDrawableShPtr GetBaseMap() const;
        void SetBaseMap(const GeoDrawableShPtr &new_map,
                        bool try_preserve_pos=true);
        void SetBaseMap(const RasterMapShPtr &new_map,
                        bool try_preserve_pos=true);

        const DisplayDeltaInt &GetDisplaySize() const;
        void SetDisplaySize(const DisplayDeltaInt &new_size);

        const OverlayList &GetOverlayList() const;
        void SetOverlayList(const OverlayList &overlay_list);

        unsigned int GetChangeCtr() const;


        BaseMapCoord
        BaseCoordFromDisplay(const DisplayCoord &disp) const;

        BaseMapCoord
        BaseCoordFromDisplay(const DisplayCoordCentered &disp) const;

        BaseMapDelta
        BaseDeltaFromDisplay(const DisplayDelta &disp) const;
};

class MapView /NoDefaultCtors/ {
%TypeHeaderCode
#include "mapdisplay.h"
%End
public:
    MapView(const DisplayShPtr &display);

    void Paint(const MapDisplayManager &mdm);
    PixelBuf PaintToBuffer(ODMPixelFormat format,
                           const MapDisplayManager &mdm);
    void ForceFullRepaint();
};

std::wstring CompassPointFromDirection(double degrees);

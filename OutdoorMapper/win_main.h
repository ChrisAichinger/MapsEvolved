#ifndef ODM__OUTDOORMAPPER_H
#define ODM__OUTDOORMAPPER_H

#include "util.h"
#include "winwrap.h"
#include "rastermap.h"

class RootWindow : public Window
{
    public:
        virtual LPCTSTR ClassName() { return TEXT("clsOutdoorMapper"); }
        virtual UINT WCStyle() { return CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW; };
        static RootWindow *Create();
        virtual void PaintContent(PAINTSTRUCT *pps);

        void MapChange();
    private:
        HWND m_hwndMap;
        HWND m_hwndStatus;
        bool m_child_drag;

        RootWindow();
        class RasterMapCollection m_maps;
        class HeightFinder m_heightfinder;
        std::shared_ptr<class MapDisplayManager> m_mapdisplay;

        std::shared_ptr<class Toolbar> m_toolbar;

        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT OnCreate();
        LRESULT OnNotify(struct NMHDRExtraData *nmh);
        void CalcMapSize(RECT &rect);
        void CreateStatusbar();
};

#endif

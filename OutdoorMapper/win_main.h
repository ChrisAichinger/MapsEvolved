#ifndef ODM__OUTDOORMAPPER_H
#define ODM__OUTDOORMAPPER_H

#include "util.h"
#include "winwrap.h"
#include "rastermap.h"

enum CursorType {
    CUR_NORMAL,
    CUR_DRAG_HAND
};

class RootWindow : public Window
{
    public:
        virtual LPCTSTR ClassName() { return TEXT("clsOutdoorMapper"); }
        virtual UINT WCStyle() {
            return CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        }
        virtual BOOL WinRegisterClass(WNDCLASS *pwc);

        static RootWindow *Create();
        virtual void PaintContent(PAINTSTRUCT *pps);

        void SetCursor(CursorType cursor);
        void UpdateStatusbar();
    private:
        HWND m_hwndMap;
        HWND m_hwndStatus;

        std::shared_ptr<class UIMode> m_mode;
        class RasterMapCollection m_maps;
        class HeightFinder m_heightfinder;
        std::shared_ptr<class MapDisplayManager> m_mapdisplay;

        std::shared_ptr<class Toolbar> m_toolbar;

        RootWindow();
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT OnCreate();
        LRESULT OnNotify(struct NMHDRExtraData *nmh);
        void CalcMapSize(RECT &rect);
        void CreateStatusbar();
        void SetSBText(int idx, const std::wstring &str);
        void SetSBText(int idx, const std::wostringstream &sstr);
};

#endif

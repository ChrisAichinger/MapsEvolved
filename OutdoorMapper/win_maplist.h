#ifndef ODM__WIN_MAPLIST_H
#define ODM__WIN_MAPLIST_H

#include <vector>

#include "util.h"
#include "winwrap.h"

class MapListSizer {
    public:
        explicit MapListSizer(HWND parent) : m_hwndParent(parent) {};
        void SetHWND(HWND hwnd) { m_hwndParent = hwnd; };

        void GetListview(RECT &rect);
        void GetTextbox(RECT &rect);
        void GetAddRasterbutton(RECT &rect);
    private:
        HWND m_hwndParent;
};

class MapListWindow : public Window
{
    public:
        explicit MapListWindow(class MapDisplayManager &mapdisplay,
                               class RasterMapCollection &maps);
        virtual LPCTSTR ClassName();
        virtual UINT WCStyle() { return CS_HREDRAW | CS_VREDRAW; };
        static MapListWindow *Create(class MapDisplayManager &mapdisplay,
                                     class RasterMapCollection &maps);
        virtual void PaintContent(PAINTSTRUCT *pps);
    private:
        class MapDisplayManager &m_mapdisplay;
        class RasterMapCollection &m_maps;
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT OnCreate();

        void InsertRow(const class RasterMap &map);

        HWND m_hwndStatic;
        HWND m_hwndBtnAddRaster;
        MapListSizer m_sizer;
        std::unique_ptr<class ListView> m_listview;
};

void ShowMapListWindow(class MapDisplayManager &mapdisplay,
                       class RasterMapCollection &maps);

#endif

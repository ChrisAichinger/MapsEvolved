#ifndef ODM__WIN_MAPLIST_H
#define ODM__WIN_MAPLIST_H

#include "util.h"
#include "winwrap.h"

class MapListSizer {
    public:
        explicit MapListSizer(HWND parent) : m_hwndParent(parent) {};
        void SetHWND(HWND hwnd) { m_hwndParent = hwnd; };

        void GetListview(RECT &rect);
        void GetTextbox(RECT &rect);
    private:
        HWND m_hwndParent;
};

class MapListWindow : public Window
{
    public:
        explicit MapListWindow(class RootWindow &rootwindow,
                               class RasterMapCollection &maps);
        virtual LPCTSTR ClassName();
        virtual UINT WCStyle() { return CS_HREDRAW | CS_VREDRAW; };
        static MapListWindow *Create(class RootWindow &rootwindow,
                                     class RasterMapCollection &maps);
        virtual void PaintContent(PAINTSTRUCT *pps);
    private:
        class RootWindow &m_rootwindow;
        class RasterMapCollection &m_maps;
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT OnCreate();

        HWND m_hwndStatic;
        HWND m_hwndListview;
        MapListSizer m_sizer;
        std::shared_ptr<ImageList> m_lvImages;
};

void ShowMapListWindow(class RootWindow &rw, class RasterMapCollection &maps);

#endif

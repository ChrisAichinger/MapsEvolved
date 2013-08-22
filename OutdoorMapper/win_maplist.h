#ifndef ODM__WIN_MAPLIST_H
#define ODM__WIN_MAPLIST_H

#include <vector>
#include <map>

#include "util.h"
#include "winwrap.h"

class MapListSizer {
    public:
        explicit MapListSizer(HWND parent) : m_hwndParent(parent) {};
        void SetHWND(HWND hwnd) { m_hwndParent = hwnd; };

        void GetListview(RECT &rect);
        void GetTextbox(RECT &rect);
        void GetAddRasterbutton(RECT &rect);
        void GetDelRasterbutton(RECT &rect);
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
    private:
        class MapDisplayManager &m_mapdisplay;
        class RasterMapCollection &m_maps;
        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT OnCreate();

        std::map<int, std::shared_ptr<const class RasterMap> > m_maps_from_item_id;
        void InsertRow(const std::shared_ptr<const class RasterMap> &map, unsigned int level);
        void InsertMaps();

        HWND m_hwndStatic;
        MapListSizer m_sizer;
        std::unique_ptr<class TreeList> m_listview;
        std::unique_ptr<class Button> m_btnAddRaster;
        std::unique_ptr<class Button> m_btnDelRaster;
};

void ShowMapListWindow(class MapDisplayManager &mapdisplay,
                       class RasterMapCollection &maps);

#endif

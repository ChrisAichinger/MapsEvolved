#include "win_maplist.h"

#include <cassert>
#include <functional>
#include <tuple>

#include "win_main.h"
#include "rastermap.h"
#include "mapdisplay.h"
#include "projection.h"
#include "resource.h"
#include "odm_config.h"

#define IDC_BTNADDRASTER      1018
#define IDC_BTNDELRASTER      1019


static const wchar_t * const MapListClassname = L"clsODM_Maplist";
LPCTSTR MapListWindow::ClassName() {
    return MapListClassname;
}

MapListWindow *MapListWindow::Create(class MapDisplayManager &mapdisplay,
                                     class RasterMapCollection &maps) {
    MapListWindow *self = new MapListWindow(mapdisplay, maps);
    if (self && self->WinCreateWindow(0,
                TEXT("OutdoorMapper Map List"), WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                NULL, NULL)) {
        return self;
    }
    delete self;
    return NULL;
}

MapListWindow::MapListWindow(MapDisplayManager &mapdisplay,
                             RasterMapCollection &maps)
    : m_mapdisplay(mapdisplay), m_maps(maps), m_sizer(0),
      m_hwndStatic(0)
{ }


LRESULT MapListWindow::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ControlInterface* controls[] = { m_btnAddRaster.get(),
                                     m_btnDelRaster.get(),
                                     m_listview.get(),
                                     m_toolbar.get(),
    };
    for (int i=0; i < ARRAY_SIZE(controls); ++i) {
        EventResult res;
        if (controls[i])
            res = controls[i]->TryHandleMessage(uMsg, wParam, lParam);
        if (res.was_handled)
            return res.result;
    }

    switch (uMsg) {
        case WM_CREATE:
            return OnCreate();
        case WM_SIZE:
            RECT rect;
            m_sizer.GetListview(rect);
            m_listview->Resize(rect);
            return 0;
        case WM_COMMAND:
            WORD id = LOWORD(wParam);
            if (id == ID_MAP_ADD) {
                HandleAddMap();
                return 0;
            }
            if (id == ID_MAP_DEL) {
                HandleDelMap(true);
                return 0;
            }
    }
    return __super::HandleMessage(uMsg, wParam, lParam);
}


LRESULT MapListWindow::OnCreate() {
    m_sizer.SetHWND(m_hwnd);

    const int IDC_TOOLBAR = 800;
    m_toolbar.reset(new Toolbar(m_hwnd, 16, IDC_TOOLBAR));
    std::list<ToolbarButton> button_list;
    ToolbarButton buttons[] = {
            ToolbarButton(IDB_MAP_ADD, ID_MAP_ADD, true, 0, L"Add Map"),
            ToolbarButton(IDB_MAP_DEL, ID_MAP_DEL, true, 0, L"Delete Map"),
            ToolbarButton(0, 0, true, ToolbarButton::SEPARATOR, L""),
            ToolbarButton(IDB_ZOOMIN, ID_MAP_ADD, false, 0, L"Not implemented"),
    };
    m_toolbar->SetButtons(std::list<ToolbarButton>(buttons, buttons + ARRAY_SIZE(buttons)));
    m_sizer.SetToolbar(m_toolbar->GetHWND());
    RECT rect;
    m_sizer.GetListview(rect);

    m_listview.reset(new TreeList());
    m_listview->Create(m_hwnd, rect);

    std::unique_ptr<ImageList> imglist(
                     new ImageList(GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   ILC_MASK | ILC_COLOR32, 10));

    imglist->AddIcon(IconHandle(g_hinst, MAKEINTRESOURCE(IDI_MAP_STD)));
    m_listview->SetImageList(std::move(imglist));

    LVCOLUMN lvcs[] = { { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM,
                          LVCFMT_LEFT,
                          200,
                          const_cast<LPWSTR>(LoadResPWCHAR(IDS_MLV_COL_FNAME)),
                        },
                        { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM,
                          LVCFMT_LEFT,
                          200,
                          const_cast<LPWSTR>(LoadResPWCHAR(IDS_MLV_COL_TYPE)),
                        },
                        { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM,
                          LVCFMT_LEFT,
                          400,
                          const_cast<LPWSTR>(LoadResPWCHAR(IDS_MLV_COL_PATH)),
                        }
                      };
    m_listview->InsertColumns(ARRAY_SIZE(lvcs), lvcs);

    ListViewEvents events;
    events.ItemChanged = [this](const ListView& lv, LPNMLISTVIEW pnmlv)
                               -> EventResult
    {
        if (!(pnmlv->uNewState & LVIS_SELECTED))
            return EventResult(false, 0);

        auto map = m_maps_from_item_id[pnmlv->iItem];
        if (map->GetType() == RasterMap::TYPE_ERROR) {
            SetWindowText(m_hwndStatic, L"Failed to open the map.");
            return EventResult(true, 0);
        }
        std::string char_proj(map->GetProj().GetProjString());
        std::wstring proj(WStringFromUTF8(char_proj));
        SetWindowText(m_hwndStatic, proj.c_str());
        return EventResult(true, 0);
    };
    events.DoubleClick = [this](const ListView& lv, LPNMITEMACTIVATE pnmia)
                               -> EventResult
    {
        if (pnmia->iItem < 0) return EventResult(true, 0);
        m_mapdisplay.ChangeMap(m_maps_from_item_id[pnmia->iItem]);
        return EventResult(true, 0);
    };
    events.RightClick = [this](const ListView& lv, LPNMITEMACTIVATE pnmia)
                               -> EventResult
    {
        if (pnmia->iItem < 0) return EventResult(true, 0);
        m_mapdisplay.AddOverlayMap(m_maps_from_item_id[pnmia->iItem]);
        return EventResult(true, 0);
    };
    events.KeyDown = [this](const ListView& lv, LPNMLVKEYDOWN pnmkd)
                               -> EventResult
    {
        if (pnmkd->wVKey == VK_DELETE) {
            HandleDelMap(false);
            return EventResult(true, 0);
        }
        return EventResult(false, 0);
    };
    events.EnterPressed = [this](const ListView& lv, LPNMHDR pnmhdr)
                               -> EventResult
    {
        int index = m_listview->GetSelectedRow();
        if (index < 0) return EventResult(true, 0);
        m_mapdisplay.ChangeMap(m_maps_from_item_id[index]);
        return EventResult(true, 0);
    };
    m_listview->RegisterEventHandlers(events);

    InsertMaps();
    ShowWindow(m_listview->GetHWND(), SW_SHOW);

    m_sizer.GetTextbox(rect);
    m_hwndStatic = CreateWindow(
            L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)0, g_hinst, NULL);

    m_sizer.GetAddRasterbutton(rect);
    m_btnAddRaster.reset(new Button());
    m_btnAddRaster->Create(m_hwnd, rect, L"Add Rastermap", 1018);
    ButtonEvents btnevents;
    btnevents.Clicked = [this](const Button& btn) -> EventResult {
        HandleAddMap();
        return EventResult(true, 0);
    };
    m_btnAddRaster->RegisterEventHandlers(btnevents);

    m_sizer.GetDelRasterbutton(rect);
    m_btnDelRaster.reset(new Button());
    m_btnDelRaster->Create(m_hwnd, rect, L"Del Rastermap", 1019);
    btnevents.Clicked = [this](const Button& btn) -> EventResult {
        HandleDelMap(true);
        return EventResult(true, 0);
    };
    m_btnDelRaster->RegisterEventHandlers(btnevents);

    return 0;
}

void MapListWindow::HandleAddMap() {
    wchar_t buf[MAX_PATH] = {0};
    wchar_t filters[] = L"Supported files\0*.tif;*.tiff\0"
                        L"Geotiff Files\0*.tif;*.tiff\0"
                        L"All Files (*.*)\0*.*\0"
                        L"\0\0";
    OPENFILENAME ofn = {
                sizeof(ofn),
                m_hwnd,
                g_hinst,
                filters,
                NULL, 0, 0,
                buf, sizeof(buf),
                NULL, 0,
                NULL, L"Open Raster Map",
                OFN_FILEMUSTEXIST,
                0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    BOOL success = GetOpenFileName(&ofn);
    if (!success) {
        int error = CommDlgExtendedError();
        return;
    }
    LoadMap(m_maps, ofn.lpstrFile);

    std::unique_ptr<PersistentStore> ps = CreatePersistentStore();
    if (!m_maps.StoreTo(ps)) {
        MessageBox(m_hwnd,
                   L"Couldn't save map preferences.\n"
                   L"Map viewing will continue to work but the "
                   L"added map will be gone when you restart.",
                   L"Outdoormapper Error", MB_OK | MB_ICONWARNING);
    }

    m_listview->DeleteAllRows();
    InsertMaps();
}

void MapListWindow::HandleDelMap(bool ErrorIfNoSelection) {
    int index = m_listview->GetSelectedRow();
    if (m_maps_from_item_id.count(index) == 0) {
        if (ErrorIfNoSelection) {
            MessageBox(m_hwnd, L"You must select a map to delete.",
                       L"Error", MB_OK | MB_ICONWARNING);
        }
        return;
    }
    std::shared_ptr<RasterMap> map = m_maps_from_item_id[index];
    if (!m_maps.IsToplevelMap(map)) {
        MessageBox(m_hwnd, L"You can only delete map files, not their sub-views.",
                   L"Error", MB_OK | MB_ICONWARNING);
        return;
    }
    m_maps.DeleteMap(index);

    std::unique_ptr<PersistentStore> ps = CreatePersistentStore();
    if (!m_maps.StoreTo(ps)) {
        MessageBox(m_hwnd,
                   L"Couldn't save map preferences.\n"
                   L"Map viewing will continue to work but the "
                   L"changes will be gone when you restart.",
                   L"Outdoormapper Error", MB_OK | MB_ICONWARNING);
    }

    m_listview->DeleteAllRows();
    InsertMaps();
}

void MapListWindow::InsertRow(const std::shared_ptr<RasterMap> &map,
                              unsigned int level)
{
    ListViewRow lvrow;
    std::wstring directory, fname;
    std::tie(directory, fname) = GetAbsPath(map->GetFname());
    lvrow.SetDepth(level);
    lvrow.AddItem(ListViewTextImageItem(fname, 0));
    wchar_t *str;
    switch (map->GetType()) {
        case GeoDrawable::TYPE_MAP:
            str = L"Map"; break;
        case GeoDrawable::TYPE_DHM:
            str = L"DHM"; break;
        case GeoDrawable::TYPE_GRADIENT_MAP:
            str = L"Gradient height map"; break;
        case GeoDrawable::TYPE_STEEPNESS_MAP:
            str = L"Steepness height map"; break;
        case GeoDrawable::TYPE_LEGEND:
            str = L"Legend"; break;
        case GeoDrawable::TYPE_OVERVIEW:
            str = L"Overview"; break;
        case GeoDrawable::TYPE_IMAGE:
            str = L"Plain image"; break;
        case GeoDrawable::TYPE_ERROR:
            str = L"Error loading map";
            lvrow.SetColor(makeRGB(0xdd, 0, 0));
            break;
        default:
            assert(false); // Unknown raster map type
    }
    lvrow.AddItem(ListViewTextItem(str));
    lvrow.AddItem(ListViewTextItem(directory));
    int index = m_listview->InsertRow(lvrow, INT_MAX);
    m_maps_from_item_id[index] = map;
}

void MapListWindow::InsertMaps() {
    std::vector<std::shared_ptr<RasterMap> > representations;
    for (unsigned int index = 0; index < m_maps.Size(); index++) {
        InsertRow(m_maps.Get(index), 0);
        auto representations = m_maps.GetAlternateRepresentations(index);
        for (auto it = representations.cbegin(); it != representations.cend(); ++it) {
            InsertRow(*it, 1);
        }
    }
}

void ShowMapListWindow(MapDisplayManager &mapdisplay,
                       RasterMapCollection &maps)
{
    HWND hwnd = FindWindowWithinProcess(MapListClassname);
    if (!hwnd) {
        hwnd = MapListWindow::Create(mapdisplay, maps)->GetHWND();
    }
    if (!SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) {
        throw std::runtime_error("Failed to activate map manager window.");
    }
}

void MapListSizer::GetListview(RECT &rect) {
    GetClientRect(rect);
    rect.left += 150;
    rect.bottom /= 2;
}

void MapListSizer::GetTextbox(RECT &rect) {
    GetClientRect(rect);
    rect.left += 150;
    rect.top += (rect.bottom - rect.top) / 2;
}

void MapListSizer::GetAddRasterbutton(RECT &rect) {
    GetClientRect(rect);
    rect.right = rect.left + 150;
    rect.bottom = rect.top + 30;
}

void MapListSizer::GetDelRasterbutton(RECT &rect) {
    GetClientRect(rect);
    rect.right = rect.left + 150;
    rect.top += 30;
    rect.bottom = rect.top + 30;
}

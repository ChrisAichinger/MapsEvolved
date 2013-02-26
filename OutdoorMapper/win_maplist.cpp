#include "win_maplist.h"

#include <cassert>
#include <functional>

#include "win_main.h"
#include "rastermap.h"
#include "mapdisplay.h"
#include "projection.h"
#include "resource.h"
#include "odm_config.h"

#define IDC_BTNADDRASTER      1018


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
    : m_mapdisplay(mapdisplay), m_maps(maps), m_sizer(0)
{ }

void MapListWindow::PaintContent(PAINTSTRUCT *pps) {
}

LRESULT MapListWindow::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:
            return OnCreate();
        case WM_NOTIFY:
            if (m_listview->HandleNotify(wParam, lParam)) {
                return 0;
            }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_BTNADDRASTER &&
                HIWORD(wParam) == BN_CLICKED &&
                (HWND)lParam == m_hwndBtnAddRaster)
            {
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
                    return 0;
                }
                LoadMap(m_maps, ofn.lpstrFile);

                m_listview->DeleteAllRows();
                for (unsigned int index = 0; index < m_maps.Size(); index++) {
                    InsertRow(m_maps.Get(index));
                }
                return 0;
            }
        }
    }
    return __super::HandleMessage(uMsg, wParam, lParam);
}


LRESULT MapListWindow::OnCreate() {
    m_sizer.SetHWND(m_hwnd);

    RECT rect;
    m_sizer.GetListview(rect);

    m_listview.reset(new ListView());
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
                        }
                      };
    m_listview->InsertColumns(ARRAY_SIZE(lvcs), lvcs);

    ListViewEvents events;
    events.ItemChanged = [this](const ListView& lv, LPNMLISTVIEW pnmlv)
                               -> LRESULT
    {
        if (!(pnmlv->uNewState & LVIS_SELECTED))
            return 0;
        const RasterMap &map = m_maps.Get(pnmlv->iItem);
        std::string char_proj(map.GetProj().GetProjString());
        std::wstring proj(WStringFromUTF8(char_proj));
        SetWindowText(m_hwndStatic, proj.c_str());
        return 0;
    };
    events.DoubleClick = [this](const ListView& lv, LPNMITEMACTIVATE pnmia)
                               -> LRESULT
    {
        m_mapdisplay.ChangeMap(&m_maps.Get(pnmia->iItem));
        return 0;
    };
    m_listview->RegisterEventHandlers(events);

    for (unsigned int index = 0; index < m_maps.Size(); index++) {
        InsertRow(m_maps.Get(index));
    }
    ShowWindow(m_listview->GetHWND(), SW_SHOW);

    m_sizer.GetTextbox(rect);
    m_hwndStatic = CreateWindow(
            L"STATIC", L"TEST",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)0, g_hinst, NULL);

    m_sizer.GetAddRasterbutton(rect);
    m_hwndBtnAddRaster = CreateWindow(
            L"BUTTON", L"Add Rastermap",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)IDC_BTNADDRASTER, g_hinst, NULL);
    return 0;
}

void MapListWindow::InsertRow(const RasterMap &map) {
    ListViewRow lvrow;
    lvrow.AddItem(ListViewTextImageItem(map.GetFname(), 0));
    wchar_t *str;
    switch (map.GetType()) {
        case RasterMap::TYPE_MAP: str = L"Map"; break;
        case RasterMap::TYPE_DHM: str = L"DHM"; break;
        case RasterMap::TYPE_GRADIENT: str = L"Gradient height map"; break;
        case RasterMap::TYPE_LEGEND: str = L"Legend"; break;
        case RasterMap::TYPE_OVERVIEW: str = L"Overview"; break;
        case RasterMap::TYPE_ERROR:
            str = L"Error loading map";
            lvrow.SetColor(makeRGB(0xdd, 0, 0));
            break;
        default:
            assert(false); // Unknown raster map type
    }
    lvrow.AddItem(ListViewTextItem(str));
    m_listview->InsertRow(lvrow, INT_MAX);
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
    GetClientRect(m_hwndParent, &rect);
    rect.left += 150;
    rect.bottom /= 2;
}

void MapListSizer::GetTextbox(RECT &rect) {
    GetClientRect(m_hwndParent, &rect);
    rect.left += 150;
    rect.top += (rect.bottom - rect.top) / 2;
}

void MapListSizer::GetAddRasterbutton(RECT &rect) {
    GetClientRect(m_hwndParent, &rect);
    rect.right = rect.left + 150;
    rect.bottom = rect.top + 30;
}

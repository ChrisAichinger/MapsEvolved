#include "win_main.h"

#include <sstream>
#include <iomanip>
#include <list>
#include <cassert>

#include "resource.h"
#include "rastermap.h"
#include "disp_ogl.h"
#include "winwrap.h"
#include "win_mappanel.h"
#include "win_maplist.h"
#include "mapdisplay.h"
#include "print_map.h"
#include "uimode.h"

#define IDC_STATUSBAR         100
#define IDC_MAP               101
#define IDC_TOOLBAR           102


static const wchar_t *MAPPATH = L"..\\..\\ok500_v5_wt99.tif";
static const wchar_t *DHMPATH = L"..\\..\\dhm.tif";


BOOL RootWindow::WinRegisterClass(WNDCLASS *pwc) {
    pwc->lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);
    return __super::WinRegisterClass(pwc);
}

void RootWindow::CreateStatusbar() {
    m_hwndStatus = CreateWindowEx(
            0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            m_hwnd, (HMENU)IDC_STATUSBAR, g_hinst, NULL);

    if (!m_hwndStatus) {
        throw std::runtime_error("Failed to create status bar.");
    }

    // Create status bar "compartments". Last -1 means that it fills the rest
    int Statwidths[] = {200, 300, 475, 600, 750, -1};
    SendMessage(m_hwndStatus, SB_SETPARTS, sizeof(Statwidths)/sizeof(int),
                reinterpret_cast<LPARAM>(Statwidths));
    SendMessage(m_hwndStatus, SB_SETTEXT, 0,
                reinterpret_cast<LPARAM>(TEXT("Hello")));
}

LRESULT RootWindow::OnCreate()
{
    CreateStatusbar();
    m_toolbar.reset(new Toolbar(m_hwnd, 16, IDC_TOOLBAR));
    ToolbarButton buttons[] = {
            ToolbarButton(IDB_DATABASE, ID_MANAGE_MAPS, true, 0, L"Manage Maps"),
            ToolbarButton(IDB_SAVEMAPBMP, ID_SAVEMAPBMP, true, 0, L"Save Map as Image"),
            ToolbarButton(IDB_PRINTER, ID_PRINT, true, 0, L"Print"),
            ToolbarButton(0, 0, true, ToolbarButton::SEPARATOR, L""),
            ToolbarButton(IDB_ZOOMIN, ID_ZOOMIN, true, 0, L"Zoom in"),
            ToolbarButton(IDB_ZOOMOUT, ID_ZOOMOUT, true, 0, L"Zoom out"),
            ToolbarButton(IDB_ZOOMEXACT, ID_ZOOMEXACT, true, 0, L"Zoom 1:1"),
    };

    m_toolbar->SetButtons(std::list<ToolbarButton>(buttons, buttons + ARRAY_SIZE(buttons)));

    m_sizer.SetHWND(m_hwnd);
    m_sizer.SetStatusbar(m_hwndStatus);
    m_sizer.SetToolbar(m_toolbar->GetHWND());

    RECT rect;
    m_sizer.GetActiveItems(rect);
    m_lv_activeitems->Create(m_hwnd, rect);
    std::unique_ptr<ImageList> imglist(
                     new ImageList(GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON),
                                   ILC_MASK | ILC_COLOR32, 10));

    imglist->AddIcon(IconHandle(g_hinst, MAKEINTRESOURCE(IDI_MAP_STD)));
    m_lv_activeitems->SetImageList(std::move(imglist));
    LVCOLUMN lvcs[] = { { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM,
                          LVCFMT_LEFT,
                          140,
                          const_cast<LPWSTR>(LoadResPWCHAR(IDS_MLV_COL_FNAME)),
    } };
    m_lv_activeitems->InsertColumns(ARRAY_SIZE(lvcs), lvcs);

    m_sizer.GetMapPanel(rect);
    m_hwndMap = CreateWindow(
            g_MapWndClass, NULL,
            WS_CHILD | WS_VISIBLE | CS_OWNDC | CS_DBLCLKS,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)IDC_MAP, g_hinst, NULL);

    if (!m_hwndMap) {
        throw std::runtime_error("Error creating map window.");
    }

    std::unique_ptr<PersistentStore> store = CreatePersistentStore();
    if (!m_maps.RetrieveFrom(store.get())) {
        MessageBox(m_hwnd,
                   L"Couldn't load map preferences.\n"
                   L"Using a default set of maps instead.",
                   L"Outdoormapper Warning", MB_OK | MB_ICONWARNING);
        LoadMap(m_maps, MAPPATH);
        LoadMap(m_maps, DHMPATH);
        LoadMap(m_maps, L"dummyfile_doesn't_exist?.tif");

        // Store newly loaded maps right back to the registry
        if (!m_maps.StoreTo(store.get())) {
            MessageBox(m_hwnd,
                       L"Couldn't save map preferences.\n"
                       L"Map viewing will continue to work but the "
                       L"added mapss will be gone when you restart.",
                       L"Outdoormapper Error", MB_OK | MB_ICONWARNING);
        }
    }

    std::shared_ptr<DevContext> dev_ctx(new DevContext(m_hwndMap));
    std::shared_ptr<OGLContext> ogl_ctx(new OGLContext(dev_ctx));
    std::shared_ptr<DispOpenGL> display(new DispOpenGL(ogl_ctx));

    m_mapdisplay.reset(new MapDisplayManager(display, m_maps.Get(0)));
    m_mode.reset(new UIModeNormal(this, m_mapdisplay));

    return 0;
}

class MouseEvent_Obj : public MouseEvent {
    public:
        explicit MouseEvent_Obj(const MW_MouseData *md) {
            x = md->xPos;
            y = md->yPos;
            flags = md->fwKeys;
            wheel_step = 1.0 * md->zDelta / WHEEL_DELTA;
        }
};

class DragEvent_Obj : public DragEvent {
    public:
        explicit DragEvent_Obj(const MW_DragData *dd) {
            start = static_cast<MouseEvent_Obj>(&dd->start);
            last = static_cast<MouseEvent_Obj>(&dd->last);
            cur = static_cast<MouseEvent_Obj>(&dd->cur);
        }
};

LRESULT RootWindow::OnNotify(struct NMHDRExtraData *nmh) {
    switch (nmh->nmhdr.idFrom) {
    case IDC_MAP:
        switch(nmh->nmhdr.code) {
            case MW_PAINT:
                m_mapdisplay->Paint();
                break;
            case MW_LCLICK: {
                MouseEvent_Obj me(reinterpret_cast<MW_MouseData*>(nmh->data));
                m_mode->OnLClick(me);
                return 0;
            }
            case MW_DBLCLK: {
                MouseEvent_Obj me(reinterpret_cast<MW_MouseData*>(nmh->data));
                m_mode->OnDblClick(me);
                return 0;
            }
            case MW_MWHEEL: {
                MouseEvent_Obj me(reinterpret_cast<MW_MouseData*>(nmh->data));
                m_mode->OnMouseWheel(me);
                return 0;
            }
            case MW_MOUSEMOVE: {
                MouseEvent_Obj me(reinterpret_cast<MW_MouseData*>(nmh->data));
                m_mode->OnMapMouseMove(me);
                return 0;
            }
            case MW_DRAGSTART: {
                DragEvent_Obj de(reinterpret_cast<MW_DragData*>(nmh->data));
                m_mode->OnDragStart(de);
                return 0;
            }
            case MW_DRAGEND: {
                DragEvent_Obj de(reinterpret_cast<MW_DragData*>(nmh->data));
                m_mode->OnDragEnd(de);
                return 0;
            }
            case MW_DRAG: {
                DragEvent_Obj de(reinterpret_cast<MW_DragData*>(nmh->data));
                m_mode->OnDrag(de);
                return 0;
            }
        }
    }
    return 0;
}

void RootWindow::PaintContent(PAINTSTRUCT *pps) {
    // Empty: we only draw the map panel, which is handled via WM_NOTIFY
}

LRESULT RootWindow::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ControlInterface* controls[] = { m_toolbar.get(),
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

        case WM_NCDESTROY:
            // Death of the root window ends the thread
            PostQuitMessage(0);
            break;

        case WM_DESTROY:
            break;

        case WM_SIZE:
            // Auto-resize statusbar (WM_SIZE does that)
            SendMessage(m_hwndStatus, WM_SIZE, 0, 0);
            if (m_hwndMap) {
                RECT rect;
                m_sizer.GetMapPanel(rect);
                SetWindowPos(m_hwndMap, NULL, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                m_mapdisplay->Resize(rect.right - rect.left,
                                     rect.bottom - rect.top);
            }
            return 0;

        case WM_SETFOCUS:
            if (m_hwndMap) {
                SetFocus(m_hwndMap);
            }
            return 0;

        case WM_NOTIFY:
            return OnNotify(reinterpret_cast<PNMHDRExtraData>(lParam));

        case WM_COMMAND:
            WORD id = LOWORD(wParam);
            if (id == ID_MANAGE_MAPS) {
                ShowMapListWindow(*m_mapdisplay, m_maps);
            }
            if (id == ID_PRINT) {
                //PageSetupDialog(m_hwnd);
                MapPrinter mprint(m_mapdisplay, 50000);
                struct PrintOrder po = { L"Outdoor Mapper Document",
                                         L"Printing...",
                                         mprint,
                                       };
                Print(m_hwnd, po);
            }
            if (id == ID_SAVEMAPBMP) {
                const RasterMap &map = m_mapdisplay->GetBaseMap();
                MapPixelCoordInt center(m_mapdisplay->GetCenter());
                MapPixelDeltaInt SaveSize(4096, 4096);
                auto pixels = map.GetRegion(center - SaveSize / 2, SaveSize);
                SaveBufferAsBMP(L"out.bmp", pixels.get(),
                                SaveSize.x, SaveSize.y, 32);
            }
            if (id == ID_ZOOMIN) {
                m_mapdisplay->StepZoom(+1);
            }
            if (id == ID_ZOOMOUT) {
                m_mapdisplay->StepZoom(-1);
            }
            if (id == ID_ZOOMEXACT) {
                m_mapdisplay->SetZoomOneToOne();
            }
    }

    return __super::HandleMessage(uMsg, wParam, lParam);
}

RootWindow::RootWindow()
    : Window(), m_hwndMap(0), m_hwndStatus(0),
      m_maps(), m_heightfinder(m_maps), m_mapdisplay(),
      m_toolbar(), m_sizer(0), m_lv_activeitems(new ListView())
{ }

RootWindow *RootWindow::Create() {
    RootWindow *self = new RootWindow();
    if (self && self->WinCreateWindow(0,
                TEXT("OutdoorMapper"), WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                NULL, NULL)) {
        return self;
    }
    delete self;
    return NULL;
}

void RootWindow::UpdateStatusbar() {
    POINT point = GetClientMousePos(m_hwndMap);
    DisplayCoord disp_p(point.x, point.y);
    MapPixelCoord base_point = m_mapdisplay->BaseCoordFromDisplay(disp_p);

    SetSBText(5, string_format(L"Zoom: %.0f %%", m_mapdisplay->GetZoom()*100));
    double mpp;
    if (MetersPerPixel(m_mapdisplay->GetBaseMap(), base_point, &mpp)) {
        SetSBText(4, string_format(L"Map: %.1f m/pix", mpp));
    } else {
        SetSBText(4, string_format(L"Unknown m/pix"));
    }

    LatLon ll;
    if (!m_mapdisplay->GetBaseMap().PixelToLatLon(base_point, &ll)) {
        SetSBText(0, L"lat/lon unknown");
        SetSBText(1, L"Height unknown");
        SetSBText(2, L"Terrain orientation unknown");
        SetSBText(3, L"Steepness unknown");
        return;
    }

    SetSBText(0, string_format(L"lat/lon = %.5f / %.5f", ll.lat, ll.lon));

    TerrainInfo ti;
    if (!m_heightfinder.CalcTerrain(ll, &ti)) {
        SetSBText(1, L"Height unknown");
        SetSBText(2, L"Terrain orientation unknown");
        SetSBText(3, L"Steepness unknown");
        return;
    }
    SetSBText(1, string_format(L"Height: %.1f m", ti.height_m));
    SetSBText(2, string_format(L"Orientation: %3s (%.1f°)",
                 CompassPointFromDirection(ti.slope_face_deg).c_str(),
                 ti.slope_face_deg));
    SetSBText(3, string_format(L"Steepness: %.1f°", ti.steepness_deg));
}

void RootWindow::SetSBText(int idx, const std::wstring &str) {
    SendMessage(m_hwndStatus, SB_SETTEXT, idx,
                reinterpret_cast<LPARAM>(str.c_str()));
}

void RootWindow::SetSBText(int idx, const std::wostringstream &sStream) {
    SendMessage(m_hwndStatus, SB_SETTEXT, idx,
                reinterpret_cast<LPARAM>(sStream.str().c_str()));
}

void RootWindow::SetCursor(CursorType cursor) {
    switch (cursor) {
        case CUR_NORMAL:    ::SetCursor(LoadCursor(NULL, IDC_ARROW)); return;
        case CUR_DRAG_HAND: ::SetCursor(LoadCursor(NULL, IDC_HAND)); return;
        default:
            assert(false); // not implemented
    }
}

void RootSizer::GetMapPanel(RECT &rect) {
    GetClientRect(rect);
    rect.left += 150;
}

void RootSizer::GetActiveItems(RECT &rect) {
    GetClientRect(rect);
    rect.right = rect.left + 150;
}

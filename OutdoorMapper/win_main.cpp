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

#define ID_OPEN_MAPLIST       0x8000
#define ID_PRINT              0x8001
#define ID_SAVEMAPBMP         0x8002


static const wchar_t *MAPPATH = L"..\\..\\ok500_v5_wt99.tif";
static const wchar_t *DHMPATH = L"..\\..\\dhm.tif";

class ToolbarButton {
    friend class Toolbar;
    public:
        ToolbarButton(int bitmap_res, int command_id, bool enabled,
                      bool checked, const std::wstring &alt_text);
    private:
        int m_bitmap_res;
        int m_command_id;
        bool m_enabled;
        bool m_checked;
        std::wstring m_alt_text;
};

class Toolbar {
    public:
        explicit Toolbar(HWND hwndParent);
        void Resize();
        void SetButtons(const std::list<ToolbarButton> &buttons);
    private:
        DISALLOW_COPY_AND_ASSIGN(Toolbar);
        HWND m_hwndParent, m_hwndTool;

        LRESULT SendMsg(UINT Msg, WPARAM wParam, LPARAM lParam);
        LRESULT SendMsg_NotNull(UINT Msg, WPARAM wParam, LPARAM lParam,
                                const char* fail_msg);
        LRESULT AddBitmap(int resource_id, unsigned int num_images);
        void FillButtonStruct(TBBUTTON *button, const ToolbarButton &tbb);

        static const int bitmapSize = 16;
};

ToolbarButton::ToolbarButton(int bitmap_res, int command_id, bool enabled,
                             bool checked, const std::wstring &alt_text)
    : m_bitmap_res(bitmap_res), m_command_id(command_id),
      m_enabled(enabled), m_checked(checked), m_alt_text(alt_text)
{}

Toolbar::Toolbar(HWND hwndParent)
    : m_hwndParent(hwndParent),
      m_hwndTool(CreateWindowEx(
            0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_WRAPABLE | TBSTYLE_TOOLTIPS |
            CCS_ADJUSTABLE | CCS_NODIVIDER,
            0, 0, 0, 0,
            hwndParent, (HMENU)IDC_TOOLBAR, g_hinst, NULL))

{
    if (!m_hwndTool) {
        throw std::runtime_error("Failed to create toolbar.");
    }

    // Tell COMCTRL which version we support; no return value to check
    SendMsg(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    // Set bitmap size
    SendMsg_NotNull(TB_SETBITMAPSIZE, 0, MAKELPARAM(bitmapSize, bitmapSize),
                    "Failed: set toolbar bitmap size");

    // Set max text rows to zero to use TBBUTTON::iString as tooltip
    SendMsg_NotNull(TB_SETMAXTEXTROWS, 0, 0, "Failed: set toolbar text rows");

    // Resize the toolbar, and then show it; no return value to check
    SendMsg(TB_AUTOSIZE, 0, 0);
}

void Toolbar::Resize() {
    SendMsg(WM_SIZE, 0, 0);
}

void Toolbar::SetButtons(const std::list<ToolbarButton> &buttons) {
    const int n_buttons = buttons.size();
    std::unique_ptr<TBBUTTON> tb_buttons(new TBBUTTON[n_buttons]);
    TBBUTTON *tb_ptr = tb_buttons.get();
    for (auto it=buttons.cbegin(); it != buttons.cend(); ++it) {
        FillButtonStruct(tb_ptr, *it);
        tb_ptr++;
    }
    SendMsg_NotNull(TB_ADDBUTTONS, (WPARAM)n_buttons, (LPARAM)tb_buttons.get(),
                    "Failed: add buttons to toolbar.");

    // Resize the toolbar, and then show it; no return value to check
    SendMsg(TB_AUTOSIZE, 0, 0);
}

LRESULT Toolbar::SendMsg(UINT Msg, WPARAM wParam, LPARAM lParam) {
    return SendMessage(m_hwndTool, Msg, wParam, lParam);
}

LRESULT Toolbar::SendMsg_NotNull(UINT Msg, WPARAM wParam, LPARAM lParam,
                                 const char* fail_msg)
{
    LRESULT res = SendMsg(Msg, wParam, lParam);
    if (!res) {
        throw std::runtime_error(fail_msg);
    }
    return res;
}

LRESULT Toolbar::AddBitmap(int resource_id, unsigned int num_images) {
    TBADDBITMAP tbaddbmp = { g_hinst, resource_id };
    LRESULT res = SendMsg(TB_ADDBITMAP, num_images, (LPARAM)&tbaddbmp);
    if (res == -1) {
        throw std::runtime_error("Failed to add bitmap to toolbar.");
    }
    return res;
}

void Toolbar::FillButtonStruct(TBBUTTON *button, const ToolbarButton &tbb) {
    memset(button, 0, sizeof(*button));
    button->iBitmap = AddBitmap(tbb.m_bitmap_res, 1);
    button->idCommand = tbb.m_command_id;
    button->fsState |= (tbb.m_enabled ? TBSTATE_ENABLED : 0);
    button->fsState |= (tbb.m_checked ? TBSTATE_CHECKED : 0);
    button->fsStyle = BTNS_AUTOSIZE;
    button->iString = (INT_PTR)tbb.m_alt_text.c_str();
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
    m_toolbar.reset(new Toolbar(m_hwnd));
    std::list<ToolbarButton> button_list;
    button_list.push_back(
            ToolbarButton(IDB_DATABASE, ID_OPEN_MAPLIST, true, false, L"Database"));
    button_list.push_back(
            ToolbarButton(IDB_PRINTER, ID_PRINT, true, false, L"Print"));
    button_list.push_back(
            ToolbarButton(IDB_SAVEMAPBMP, ID_SAVEMAPBMP, true, false, L"Save Map as Image"));
    m_toolbar->SetButtons(button_list);

    RECT rect;
    CalcMapSize(rect);
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
                   L"Outdoormapper Error", MB_OK | MB_ICONWARNING);
        LoadMap(m_maps, MAPPATH);
        LoadMap(m_maps, DHMPATH);
        LoadMap(m_maps, L"dummyfile_doesn't_exist?.tif");
    }

    std::shared_ptr<DevContext> dev_ctx(new DevContext(m_hwndMap));
    std::shared_ptr<OGLContext> ogl_ctx(new OGLContext(dev_ctx));
    std::shared_ptr<DispOpenGL> display(new DispOpenGL(ogl_ctx));

    m_mapdisplay.reset(new MapDisplayManager(display, m_maps));
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

void RootWindow::CalcMapSize(RECT &rect) {
    const int info[] = { -1, -1,
                         -1, IDC_STATUSBAR,
                         -1, IDC_TOOLBAR,
                         0, 0 };
    GetEffectiveClientRect(m_hwnd, &rect, info);
}

LRESULT RootWindow::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
            // Auto-resize statusbar and toolbar (WM_SIZE does that)
            SendMessage(m_hwndStatus, WM_SIZE, 0, 0);
            m_toolbar->Resize();
            if (m_hwndMap) {
                RECT rect;
                CalcMapSize(rect);
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
            if (id == ID_OPEN_MAPLIST) {
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
    }

    return __super::HandleMessage(uMsg, wParam, lParam);
}

RootWindow::RootWindow()
    : Window(), m_hwndMap(0), m_hwndStatus(0),
      m_maps(), m_heightfinder(m_maps), m_mapdisplay(),
      m_toolbar()
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
        SetSBText(4, string_format(L"Uknown m/pix"));
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

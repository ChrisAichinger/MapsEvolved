
#include <sstream>
#include <iomanip>

#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>

#include "resource.h"
#include "rastermap.h"
#include "disp_ogl.h"
#include "win_main.h"
#include "winwrap.h"
#include "win_mappanel.h"
#include "win_maplist.h"
#include "mapdisplay.h"
#include "print_map.h"

#define IDC_STATUSBAR     100
#define IDC_MAP           101
#define IDC_TOOLBAR       102

#define ID_FILE_NEW       0x8000
#define ID_FILE_OPEN      0x8001
#define ID_FILE_SAVEAS    0x8002


static const wchar_t *MAPPATH = L"..\\..\\ok500_v5_wt99.tif";
static const wchar_t *DHMPATH = L"..\\..\\dhm.tif";

class Toolbar {
    public:
        explicit Toolbar(HWND hwndParent);
        void Resize();
    private:
        DISALLOW_COPY_AND_ASSIGN(Toolbar);
        HWND m_hwndParent, m_hwndTool;
        ImageList m_imagelist;

        static const int bitmapSize = 16;
        static const int n_buttons  = 3;
};

Toolbar::Toolbar(HWND hwndParent)
    : m_hwndParent(hwndParent),
      m_imagelist(bitmapSize, bitmapSize, ILC_COLOR16 | ILC_MASK, n_buttons),
      m_hwndTool(CreateWindowEx(
            0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | TBSTYLE_WRAPABLE | TBSTYLE_TOOLTIPS |
            CCS_ADJUSTABLE | CCS_NODIVIDER,
            0, 0, 0, 0,
            hwndParent, (HMENU)IDC_TOOLBAR, g_hinst, NULL))

{
    static HIMAGELIST g_hImageList;

    if (m_hwndTool == NULL) {
        throw std::runtime_error("Failed to create toolbar.");
    }

    // Only using one imagelist (id = 0) for now; multiple imagelists might be
    // useful to load some items from COMCTRL32 and some from the application.
    // msdn.microsoft.com/en-us/library/windows/desktop/bb787433%28v=vs.85%29
    const int ImageListID = 0;
    SendMessage(m_hwndTool, TB_SETIMAGELIST,
                (WPARAM)ImageListID, (LPARAM)m_imagelist.Get());

    SendMessage(m_hwndTool, TB_LOADIMAGES,
                (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);

    const DWORD btnStyles = BTNS_AUTOSIZE;
    TBBUTTON tbButtons[n_buttons] =
    {
        { MAKELONG(STD_FILENEW,  ImageListID),
          ID_FILE_NEW,  TBSTATE_ENABLED, btnStyles, {0}, 0, (INT_PTR)L"TT1" },
        { MAKELONG(STD_FILEOPEN, ImageListID),
          ID_FILE_OPEN, TBSTATE_ENABLED, btnStyles, {0}, 0, (INT_PTR)L"TT2" },
        { MAKELONG(STD_FILESAVE, ImageListID),
          ID_FILE_SAVEAS, 0, btnStyles, {0}, 0, (INT_PTR)L"TT3"}
    };

    // Set max text rows to zero to use TBBUTTON::iString as tooltip
    SendMessage(m_hwndTool, TB_SETMAXTEXTROWS, 0, 0);

    // Add buttons.
    SendMessage(m_hwndTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(m_hwndTool, TB_ADDBUTTONS, (WPARAM)n_buttons,
                                           (LPARAM)&tbButtons);

    // Resize the toolbar, and then show it.
    SendMessage(m_hwndTool, TB_AUTOSIZE, 0, 0);
}

void Toolbar::Resize() {
    SendMessage(m_hwndTool, WM_SIZE, 0, 0);
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

    LoadMap(m_maps, MAPPATH);
    LoadMap(m_maps, DHMPATH);

    std::shared_ptr<DevContext> dev_ctx(new DevContext(m_hwndMap));
    std::shared_ptr<OGLContext> ogl_ctx(new OGLContext(dev_ctx));
    std::shared_ptr<DispOpenGL> display(new DispOpenGL(ogl_ctx));

    m_mapdisplay.reset(new MapDisplayManager(display, m_maps));

    m_child_drag = false;
    return 0;
}

LRESULT RootWindow::OnNotify(struct NMHDRExtraData *nmh) {
    switch (nmh->nmhdr.idFrom) {
    case IDC_MAP:
        switch(nmh->nmhdr.code) {
            case MW_PAINT:
                m_mapdisplay->Paint();
                break;
            case MW_DBLCLK: {
                POINT mouse_pos = GetClientMousePos(m_hwndMap);
                m_mapdisplay->CenterToDisplayCoord(mouse_pos.x,
                                                   mouse_pos.y);
                return 0;
            }
            case MW_MWHEEL: {
                POINT point = GetClientMousePos(m_hwndMap);
                int step = GET_WHEEL_DELTA_WPARAM(nmh->data) / WHEEL_DELTA;
                m_mapdisplay->StepZoom(step, point.x, point.y);
                UpdateStatusbar();
                return 0;
                }
            case MW_LCLICK:
                MessageBeep(MB_ICONERROR);
                return 0;
            case MW_DRAGSTART:
                m_child_drag = true;
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return 0;
            case MW_DRAGEND:
                m_child_drag = false;
                return 0;
            case MW_DRAG: {
                PMW_DragStruct pdrag = (PMW_DragStruct)nmh->data;
                m_mapdisplay->DragMap(pdrag->cur.x - pdrag->last.x,
                                      pdrag->cur.y - pdrag->last.y);
                return 0;
            }
            case MW_MOUSEMOVE:
                UpdateStatusbar();
                return 0;
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
            return OnNotify((PNMHDRExtraData)lParam);
        case WM_COMMAND:
            WORD id = LOWORD(wParam);
            if (id == ID_FILE_NEW) ShowMapListWindow(*m_mapdisplay, m_maps);
            if (id == ID_FILE_OPEN) {
                //PageSetupDialog(m_hwnd);
                MapPrinter mprint(m_mapdisplay, 50000);
                struct PrintOrder po = { L"Outdoor Mapper Document",
                                         L"Printing...",
                                         mprint,
                                       };
                Print(m_hwnd, po);
            }
    }

    return __super::HandleMessage(uMsg, wParam, lParam);
}

RootWindow::RootWindow()
    : Window(), m_hwndMap(0), m_hwndStatus(0), m_child_drag(false),
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
    Point<int> disp_point(point.x, point.y);
    Point<double> base_point(
            m_mapdisplay->BaseCoordFromDisplayCoord(disp_point));

    double x = base_point.GetX();
    double y = base_point.GetY();
    m_mapdisplay->GetBaseMap().PixelToLatLong(&x, &y);

    SetSBText(0, string_format(L"lat/lon = %.5f / %.5f", y, x));
    SetSBText(5, string_format(L"Zoom: %.0f %%", m_mapdisplay->GetZoom()*100));
    double mpp = MetersPerPixel(&m_mapdisplay->GetBaseMap(),
                                base_point.GetX(), base_point.GetY());
    SetSBText(4, string_format(L"Map: %.1f m/pix", mpp));

    double height, orientation, steepness;
    if (!m_heightfinder.CalcTerrain(x, y, &height, &orientation, &steepness)) {
        SetSBText(1, L"Height unknown");
        SetSBText(2, L"Terrain orientation unknown");
        SetSBText(3, L"Steepness unknown");
        return;
    }
    SetSBText(1, string_format(L"Height: %.1f m", height));
    SetSBText(2, string_format(L"Orientation: %3s (%.1f°)",
                 CompassPointFromDirection(orientation).c_str(), orientation));
    SetSBText(3, string_format(L"Steepness: %.1f°", steepness));
}

void RootWindow::SetSBText(int idx, const std::wstring &str) {
    SendMessage(m_hwndStatus, SB_SETTEXT, idx,
                reinterpret_cast<LPARAM>(str.c_str()));
}

void RootWindow::SetSBText(int idx, const std::wostringstream &sStream) {
    SendMessage(m_hwndStatus, SB_SETTEXT, idx,
                reinterpret_cast<LPARAM>(sStream.str().c_str()));
}

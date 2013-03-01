#include "win_mappanel.h"

#include <cassert>
#include <stdexcept>

#include <Windowsx.h>

#include "winwrap.h"

extern HINSTANCE g_hinst;

IMapWindow::~IMapWindow() {};

class MapWindow : public IMapWindow {
    public:
        MapWindow(HWND hwnd);

        static ATOM RegisterControl();

        virtual void GetSize(int& w, int& h);
        virtual void Resize();

    private:
        HWND m_hwnd;
        HMENU m_hmenu;

        bool m_child_drag;
        MW_DragData m_dragdata;

        static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

        //void OnCommand(int cmd);
        //void OnContextMenu(int x, int y);

        LRESULT NotifyParent(UINT code, UINT_PTR data = 0);
        void InitMouseData(MW_MouseData *md, WPARAM wParam,
                           LPARAM lParam, bool is_client_coord = true);
};

IMapWindow *GetIMapWindow(HWND hwnd) {
    return (IMapWindow *)(MapWindow *)GetWindowLongPtr(hwnd, 0);
}

bool RegisterMapWindow() {
    return !!MapWindow::RegisterControl();
}

ATOM MapWindow::RegisterControl() {
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = MapWindow::s_WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(MapWindow *);
    wc.hInstance     = g_hinst;
    wc.hIcon         = NULL;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; //(HBRUSH)(COLOR_3DFACE+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_MapWndClass;

    return RegisterClass(&wc);
}

LRESULT CALLBACK MapWindow::s_WndProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        MapWindow *pmapwindow = new MapWindow(hwnd);
        if (!pmapwindow) {
            return FALSE;
        }
        SetWindowLongPtr(hwnd, 0, (LONG_PTR)pmapwindow);
    } else if (msg == WM_NCDESTROY) {
        delete reinterpret_cast<MapWindow *>(GetWindowLongPtr(hwnd, 0));
        SetWindowLongPtr(hwnd, 0, NULL);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    MapWindow *pmw = reinterpret_cast<MapWindow *>(GetWindowLongPtr(hwnd, 0));
    return pmw->HandleMessage(msg, wParam, lParam);
}

MapWindow::MapWindow(HWND hwnd)
    : m_hwnd(hwnd), m_hmenu(0), m_child_drag(false), m_dragdata()
{ }

void MapWindow::InitMouseData(MW_MouseData *md, WPARAM wParam,
                              LPARAM lParam, bool is_client_coord)
{
    if (is_client_coord) {
        md->xPos = GET_X_LPARAM(lParam);
        md->yPos = GET_Y_LPARAM(lParam);
    } else {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(m_hwnd, &pt);
        md->xPos = pt.x;
        md->yPos = pt.y;
    }
    md->fwKeys = GET_KEYSTATE_WPARAM(wParam);
    md->zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
}

LRESULT MapWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_PAINT: {
            PaintContext paint(m_hwnd);
            NotifyParent(MW_PAINT);
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            MW_MouseData md;
            InitMouseData(&md, wParam, lParam);
            return NotifyParent(MW_DBLCLK, reinterpret_cast<UINT_PTR>(&md));
        }
        case WM_MOUSEWHEEL: {
            MW_MouseData md;
            InitMouseData(&md, wParam, lParam, false);
            return NotifyParent(MW_MWHEEL, reinterpret_cast<UINT_PTR>(&md));
        }
        case WM_CONTEXTMENU:
            assert(false); // Not implemented
            //OnContextMenu((sint16)LOWORD(lParam), (sint16)HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            assert(false); // Not implemented
            //OnCommand(LOWORD(wParam));
            break;

        case WM_LBUTTONDOWN: {
            MW_MouseData md;
            InitMouseData(&md, wParam, lParam);

            POINT point = { md.xPos, md.yPos };
            ClientToScreen(m_hwnd, &point);
            if (DragDetect(m_hwnd, point)) {
                m_dragdata.last = m_dragdata.cur = m_dragdata.start = md;
                m_child_drag = true;
                SetCapture(m_hwnd);
                NotifyParent(MW_DRAGSTART,
                             reinterpret_cast<UINT_PTR>(&m_dragdata));
            } else {
                NotifyParent(MW_LCLICK, reinterpret_cast<UINT_PTR>(&md));
            }
            return 0;
        }

        case WM_CANCELMODE:
        case WM_LBUTTONUP:
            if (m_child_drag) {
                ReleaseCapture();
                return 0;
            }
            break;

        case WM_CHAR:
            if (m_child_drag && wParam == VK_ESCAPE) {
                ReleaseCapture();
                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            // Capture released/lost, clean up
            if (m_child_drag) {
                m_child_drag = ((HWND)lParam == m_hwnd);
                if (!m_child_drag) {
                    NotifyParent(MW_DRAGEND,
                                 reinterpret_cast<UINT_PTR>(&m_dragdata));
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (m_child_drag) {
                InitMouseData(&m_dragdata.cur, wParam, lParam);
                NotifyParent(MW_DRAG, reinterpret_cast<UINT_PTR>(&m_dragdata));
                m_dragdata.last = m_dragdata.cur;
            } else {
                MW_MouseData md;
                InitMouseData(&md, wParam, lParam);
                NotifyParent(MW_MOUSEMOVE, reinterpret_cast<UINT_PTR>(&md));
            }
            return 0;
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

void MapWindow::GetSize(int& w, int& h) { assert(false); }
void MapWindow::Resize() { assert(false); }

LRESULT MapWindow::NotifyParent(UINT code, UINT_PTR data) {
    NMHDRExtraData hdr;

    hdr.nmhdr.hwndFrom = m_hwnd;
    hdr.nmhdr.idFrom = GetWindowLong(m_hwnd, GWL_ID);
    hdr.nmhdr.code = code;
    hdr.data = data;

    return SendMessage(GetParent(m_hwnd), WM_NOTIFY,
                       (WPARAM)hdr.nmhdr.idFrom, (LPARAM)&hdr);
}



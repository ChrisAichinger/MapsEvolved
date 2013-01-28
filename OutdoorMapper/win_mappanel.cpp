#include <assert.h>
#include <stdexcept>

#include <Windows.h>
#include <Windowsx.h>
#include "win_mappanel.h"
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
        MW_DragStruct m_dragstr;

        static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

        //void OnCommand(int cmd);
        //void OnContextMenu(int x, int y);

        LRESULT NotifyParent(UINT code, UINT_PTR data = 0);
};

IMapWindow *GetIMapWindow(HWND hwnd) {
    return (IMapWindow *)(MapWindow *)GetWindowLongPtr(hwnd, 0);
}

ATOM RegisterMapWindow() {
    ATOM res = MapWindow::RegisterControl();
    if (!res) {
        throw std::runtime_error("Failed to register map window class.");
    }
    return res;
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
    : m_hwnd(hwnd), m_hmenu(0), m_child_drag(false), m_dragstr()
{ }

LRESULT MapWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_PAINT: {
            PaintContext paint(m_hwnd);
            NotifyParent(MW_PAINT);
            return 0;
        }
        case WM_LBUTTONDBLCLK: return NotifyParent(MW_DBLCLK);
        case WM_MOUSEWHEEL:    return NotifyParent(MW_MWHEEL, wParam);
        case WM_CONTEXTMENU:
            assert(false); // Not implemented
            //OnContextMenu((sint16)LOWORD(lParam), (sint16)HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            assert(false); // Not implemented
            //OnCommand(LOWORD(wParam));
            break;

        case WM_LBUTTONDOWN: {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(m_hwnd, &point);
            if (DragDetect(m_hwnd, point)) {
                POINT mouse_pos;
                GetCursorPos(&mouse_pos);
                m_dragstr.start = m_dragstr.last = m_dragstr.cur = mouse_pos;
                m_child_drag = true;

                SetCapture(m_hwnd);
                NotifyParent(MW_DRAGSTART, (UINT_PTR)(&m_dragstr));
            } else {
                NotifyParent(MW_LCLICK);
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
                    POINT mouse_pos;
                    GetCursorPos(&mouse_pos);
                    m_dragstr.cur = mouse_pos;
                    NotifyParent(MW_DRAGEND, (UINT_PTR)(&m_dragstr));
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (m_child_drag) {
                POINT mouse_pos;
                GetCursorPos(&mouse_pos);
                m_dragstr.cur = mouse_pos;
                LRESULT res = NotifyParent(MW_DRAG, (UINT_PTR)(&m_dragstr));
                m_dragstr.last = mouse_pos;
                return res;
            } else {
                return NotifyParent(MW_MOUSEMOVE);
            }
            break;
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



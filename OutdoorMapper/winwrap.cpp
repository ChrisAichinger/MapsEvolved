#include <stdexcept>
#include <assert.h>

#include <windows.h>
#include <GL/gl.h>                      /* OpenGL header file */
#include <GL/glu.h>                     /* OpenGL utilities header file */

#include "winwrap.h"


DevContext::DevContext(HWND hwnd)
    : m_hwnd(hwnd), m_hdc(GetDC(m_hwnd))
{ 
    if (!m_hwnd)
        throw std::invalid_argument("Invalid window handle (NULL)");
    if (!m_hdc)
        throw std::runtime_error("Getting device context failed");
}

DevContext::~DevContext() {
    ReleaseDC(m_hwnd, m_hdc);
}

void DevContext::SetPixelFormat() {
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;

    int pf = ChoosePixelFormat(m_hdc, &pfd);
    if (pf == 0) {
        throw std::runtime_error("No suitable pixel format.");
    } 

    if (::SetPixelFormat(m_hdc, pf, &pfd) == FALSE) {
        throw std::runtime_error("Cannot set pixel format.");
    } 
}


OGLContext::OGLContext(const std::shared_ptr<DevContext> &device)
    : m_device(device)
{ 
    m_device->SetPixelFormat();
    m_hglrc = wglCreateContext(m_device->Get());
    wglMakeCurrent(device->Get(), m_hglrc);
}

OGLContext::~OGLContext() {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_hglrc);
}


PaintContext::PaintContext(HWND hwnd) :
    m_hwnd(hwnd), m_ps()
{
    m_hdc = BeginPaint(m_hwnd, &m_ps);
    if (!m_hdc) {
        throw std::runtime_error("Failed to begin paint.");
    }
}

PaintContext::~PaintContext() {
    EndPaint(m_hwnd, &m_ps);
}


ImageList::ImageList(int width, int height, unsigned int flags, int length)
    : m_handle(ImageList_Create(width, height, flags, length, 0))
{
    if (!m_handle) {
        throw std::runtime_error("Failed to create image list.");
    }
}

ImageList::~ImageList() {
    // FIXME: Ignore return value, nothing to do about it anyway.. Log maybe
    assert(ImageList_Destroy(m_handle));
}


IconHandle::IconHandle(HINSTANCE hInstance, LPCTSTR lpIconName)
    : m_handle((HICON)LoadImage(hInstance, lpIconName, IMAGE_ICON, 0, 0, 0))
{ 
    if (!m_handle) {
        throw std::runtime_error("Failed to load icon.");
    }
}

IconHandle::~IconHandle() {
    DestroyIcon(m_handle);
}


BitmapHandle::BitmapHandle(HINSTANCE hInstance, LPCTSTR lpBitmapName)
    : m_handle((HBITMAP)LoadImage(hInstance, lpBitmapName, IMAGE_BITMAP, 0, 0, 0))
{ 
    if (!m_handle) {
        throw std::runtime_error("Failed to load bitmap.");
    }
}

BitmapHandle::~BitmapHandle() {
    DeleteObject(m_handle);
}


void Window::Register()
{
    WNDCLASS wc;
    wc.style         = WCStyle();
    wc.lpfnWndProc   = Window::s_WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hinst;
    wc.hIcon         = NULL;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = ClassName();

    WinRegisterClass(&wc);
}

LRESULT CALLBACK Window::s_WndProc(
        HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Window *self;
    if (uMsg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<Window *>(lpcs->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                reinterpret_cast<LPARAM>(self));
    } else {
        self = reinterpret_cast<Window *>
            (GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->HandleMessage(uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT Window::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lres;

    switch (uMsg) {
        case WM_NCDESTROY:
            lres = DefWindowProc(m_hwnd, uMsg, wParam, lParam);
            SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
            delete this;
            return lres;

        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_PRINTCLIENT:
            OnPrintClient(reinterpret_cast<HDC>(wParam));
            return 0;
    }

    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void Window::OnPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(m_hwnd, &ps);
    PaintContent(&ps);
    EndPaint(m_hwnd, &ps);
}

void Window::OnPrintClient(HDC hdc)
{
    PAINTSTRUCT ps;
    ps.hdc = hdc;
    GetClientRect(m_hwnd, &ps.rcPaint);
    PaintContent(&ps);
}


HWND Window::WinCreateWindow(DWORD dwExStyle, LPCTSTR pszName,
        DWORD dwStyle, int x, int y, int cx, int cy,
        HWND hwndParent, HMENU hmenu)
{
    Register();
    return CreateWindowEx(dwExStyle, ClassName(), pszName, dwStyle,
            x, y, cx, cy, hwndParent, hmenu, g_hinst, this);
}


POINT GetClientMousePos(HWND hwnd) {
    POINT point = {0, 0};
    if (!GetCursorPos(&point)) {
        throw std::runtime_error("Failed to get cursor position.");
    }
    if (!ScreenToClient(hwnd, &point)) {
        throw std::runtime_error("Failed to map client points.");
    }
    return point;
}

struct FWWP_struct {
    const std::wstring &cls_name;
    const DWORD pid;
    HWND hwnd;
};

static BOOL CALLBACK FWWP_EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    FWWP_struct *data = (FWWP_struct *)lParam;
    DWORD wndProcess;
    GetWindowThreadProcessId(hwnd, &wndProcess);
    if (wndProcess != data->pid)
        return true; // continue iteration
    
    wchar_t wndClsName[1024];
    GetClassName(hwnd, wndClsName, ARRAY_SIZE(wndClsName));
    if (wndClsName != data->cls_name)
        return true; // continue iteration
    
    data->hwnd = hwnd;
    return false; // exit immediately
}

HWND FindWindowWithinProcess(const std::wstring &wndClass) {
    FWWP_struct data = { wndClass, GetCurrentProcessId(), 0 };

    // No error checking here as data.hwnd is initialized to 0 anyway
    EnumWindows(FWWP_EnumWindowsProc, (LPARAM)&data);
    return data.hwnd;
}

std::wstring LoadResString(UINT uID, HINSTANCE hInst) {
    if (hInst == (HINSTANCE)-1) {
        hInst = g_hinst;
    }

    // Retrieve pointer to string resource
    const wchar_t *p_str_resource = NULL;
    LoadString(hInst, uID, (LPWSTR)&p_str_resource, 0);

    return std::wstring(p_str_resource);
}

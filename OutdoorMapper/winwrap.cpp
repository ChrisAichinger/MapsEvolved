#include <stdexcept>
#include <assert.h>

#include <windows.h>
#include <GL/gl.h>                      /* OpenGL header file */
#include <GL/glu.h>                     /* OpenGL utilities header file */

#include "winwrap.h"
#include "odm_config.h"

TemporaryWindowDisable::TemporaryWindowDisable(HWND hwnd)
    : m_hwnd(hwnd)
{
    assert(m_hwnd);
    EnableWindow(hwnd, false);
}

TemporaryWindowDisable::~TemporaryWindowDisable() {
    EnableNow();
}

void TemporaryWindowDisable::EnableNow() {
    if (m_hwnd) {
        EnableWindow(m_hwnd, true);
        m_hwnd = NULL;
    }
}

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

void DevContext::ForceRepaint() {
    InvalidateRect(m_hwnd, NULL, false);
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
    : m_handle((HBITMAP)LoadImage(hInstance, lpBitmapName, IMAGE_BITMAP,
                                  0, 0, 0))
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


/////////////////////////////
// Print abort implementation
/////////////////////////////

class PrintAbortImpl {
    public:
        PrintAbortImpl(PrintAbortManager *pam, HDC hdc)
            : m_pam(pam), m_hdc(hdc) {}
        ~PrintAbortImpl() { m_pam->ReturnTicket(m_hdc); }
    private:
        PrintAbortManager *m_pam;
        HDC m_hdc;
};


PrintAbortTicket::PrintAbortTicket(class PrintAbortManager *pam, HDC hdc)
    : m_impl(std::shared_ptr<PrintAbortImpl>(new PrintAbortImpl(pam, hdc)))
{}


PrintAbortManager::PrintAbortManager()
    : m_hdc(NULL), m_pdlg(NULL)
{}

PrintAbortTicket
PrintAbortManager::RegisterAbort(HDC hdc, IPrintAbortHandler *pdlg) {
    assert(!m_hdc && !m_pdlg);
    m_hdc = hdc;
    m_pdlg = pdlg;
    SetAbortProc(m_hdc, s_AbortProc);
    return PrintAbortTicket(this, hdc);
}

void PrintAbortManager::ReturnTicket(HDC hdc) {
    assert(hdc == m_hdc && m_hdc && m_pdlg);
    m_hdc = NULL;
    m_pdlg = NULL;
}

BOOL CALLBACK PrintAbortManager::s_AbortProc(HDC hdcPrn, int iCode) {
    PrintAbortManager *self = PrintAbortManager::Instance();
    assert(self->m_hdc && self->m_pdlg);
    assert(self->m_hdc == hdcPrn);
    return self->m_pdlg->PrintAbortCallback(iCode);
}

PrintAbortManager *PrintAbortManager::Instance() {
    static PrintAbortManager *print_manager = new PrintAbortManager();
    return print_manager;
}

////////////////////////////////////////
// Print Canceling Dialog implementation
////////////////////////////////////////

PrintDialog::PrintDialog(HWND hwnd_parent, TemporaryWindowDisable *twd,
                         const std::wstring &app_name)
    : m_hwnd_parent(hwnd_parent), m_twd(twd), m_user_abort(false),
      m_app_name(app_name)
{
    m_hwnd = CreateDialogParam(g_hinst, L"PrintDlgBox", hwnd_parent,
                               s_PrintDlgProc, reinterpret_cast<LPARAM>(this));
    if (!m_hwnd)
        throw std::runtime_error("Failed to create print dialog");
}

PrintDialog::~PrintDialog() {
    Close();
}

void PrintDialog::Close() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool PrintDialog::PrintAbortCallback(int iCode) {
    MSG msg;
    while (!m_user_abort && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (!m_hwnd || !IsDialogMessage(m_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return !m_user_abort;
}

BOOL CALLBACK PrintDialog::s_PrintDlgProc(HWND hDlg, UINT message,
                                          WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
    }
    PrintDialog *self = reinterpret_cast<PrintDialog*>(
                        GetWindowLongPtr(hDlg, GWLP_USERDATA));
    if (!self)
        return false;

    BOOL result = self->PrintDlgProc(message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        self->m_hwnd = 0;
    }
    return result;
}

BOOL PrintDialog::PrintDlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            SetWindowText(m_hwnd, m_app_name.c_str());
            EnableMenuItem(GetSystemMenu(m_hwnd, FALSE), SC_CLOSE, MF_GRAYED);
            return true;

        case WM_COMMAND:
            m_user_abort = true;
            m_twd->EnableNow();
            Close();
            return false;
    }
    return false;
}

class PrinterPreferences {
    public:
        PrinterPreferences();
        bool ShowDialog(HWND hwnd_parent);
        HDC GetHDC();
        bool CheckSupportRaster();
    private:
        PRINTDLGEX m_pd;
        GlobalMem m_dev_mode;
        GlobalMem m_dev_names;
        PrinterDC m_printer_dc;
};

PrinterPreferences::PrinterPreferences()
    : m_pd(), m_dev_mode(NULL), m_dev_names(NULL), m_printer_dc(NULL)
{}

bool PrinterPreferences::ShowDialog(HWND hwnd_parent) {
    m_pd.lStructSize = sizeof(m_pd);
    m_pd.hwndOwner = hwnd_parent;
    m_pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOPAGENUMS;
    m_pd.nStartPage = START_PAGE_GENERAL;

    HRESULT res = PrintDlgEx(&m_pd);
    m_dev_mode.Reset(m_pd.hDevMode);
    m_dev_names.Reset(m_pd.hDevNames);
    m_printer_dc.Reset(m_pd.hDC);

    if (res != S_OK)
        throw std::runtime_error("Unknown printer selection dialog error");

    if (m_pd.dwResultAction == PD_RESULT_CANCEL)
        return false;

    if (m_pd.dwResultAction == PD_RESULT_APPLY) {
        assert(false); // Not implemented
        return false;
    }

    if (!m_pd.hDC)
        throw std::runtime_error("HDC returned by PrintDlg invalid");

    return true;
}

HDC PrinterPreferences::GetHDC() {
    return m_printer_dc.Get();
}

bool PrinterPreferences::CheckSupportRaster() {
    return (GetDeviceCaps(m_printer_dc.Get(), RASTERCAPS) & RC_BITBLT) != 0;
}

bool Print(HWND hwnd_parent, const struct PrintOrder &order) {
    PrinterPreferences pref;
    if (!pref.ShowDialog(hwnd_parent)) {
        // user aborted the operation
        return false;
    }
    if (!pref.CheckSupportRaster())
        throw std::runtime_error("Printer does not support bitmap printing");

    HDC hdc = pref.GetHDC();
    int horzsize = GetDeviceCaps(hdc, HORZSIZE);
    int vertsize = GetDeviceCaps(hdc, VERTSIZE);
    int horzres = GetDeviceCaps(hdc, HORZRES);
    int vertres = GetDeviceCaps(hdc, VERTRES);
    int aspectx = GetDeviceCaps(hdc, ASPECTX);
    int aspecty = GetDeviceCaps(hdc, ASPECTY);
    int aspectxy = GetDeviceCaps(hdc, ASPECTXY);
    int pwidth = GetDeviceCaps(hdc, PHYSICALWIDTH);
    int pheight = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    int poffx = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    int poffy = GetDeviceCaps(hdc, PHYSICALOFFSETY);
    int scalex = GetDeviceCaps(hdc, SCALINGFACTORX);
    int scaley = GetDeviceCaps(hdc, SCALINGFACTORY);
    int dpi_x = round_to_int(INCH_to_MM * horzres / horzsize);
    int dpi_y = round_to_int(INCH_to_MM * vertres / vertsize);

    TemporaryWindowDisable twd(hwnd_parent);
    PrintDialog pd(hwnd_parent, &twd, order.PrintDialogName);
    PrintAbortTicket pad = PrintAbortManager::Instance()->RegisterAbort(hdc,
                                                                        &pd);

    static DOCINFO di = { 0 };
    di.cbSize = sizeof(di);
    di.lpszDocName = order.DocName.c_str();
    if (StartDoc(hdc, &di) <= 0)
        return false;

    if (StartPage(hdc) <= 0)
        return false;

    order.print_client.Print(hdc);

    if (EndPage(hdc) <= 0)
        return false;

    if (EndDoc(hdc) <= 0)
        return false;

    return true;
}

bool PageSetupDialog(HWND hwnd_parent) {
    PAGESETUPDLG psd = { 0 };
    psd.lStructSize = sizeof(psd);
    psd.hwndOwner = hwnd_parent;

    if (!PageSetupDlg(&psd)) {
        if (CommDlgExtendedError() == 0) {
            // User canceled the print dialog
            return false;
        }
        throw std::runtime_error("Unknown page setup dialog error");
    }

    if (psd.hDevMode)
        GlobalFree(psd.hDevMode);

    if (psd.hDevNames)
        GlobalFree(psd.hDevNames);
    return true;
}


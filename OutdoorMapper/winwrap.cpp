#include "winwrap.h"

#include <stdexcept>
#include <cassert>

#include <Windows.h>        // Not used directly but needed for OpenGL includes
#include <GL/gl.h>          // OpenGL header file
#include <GL/glu.h>         // OpenGL utilities header file


#include "odm_config.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
  name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
  processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

HINSTANCE g_hinst = 0;


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
    return std::wstring(LoadResPWCHAR(uID, hInst));
}

const wchar_t *LoadResPWCHAR(UINT uID, HINSTANCE hInst) {
    if (hInst == (HINSTANCE)-1) {
        hInst = g_hinst;
    }

    // Retrieve pointer to string resource
    const wchar_t *p_str_resource = NULL;
    LoadString(hInst, uID, (LPWSTR)&p_str_resource, 0);
    return p_str_resource;
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

bool FileExists(const wchar_t* fname) {
  DWORD dwAttrib = GetFileAttributes(fname);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


struct DottedLineParams {
    HDC hdc;
    COLORREF color;
    const RECT* cliprect;
};
void CALLBACK DottedLineCB(int x, int y, LPARAM data) {
    const DottedLineParams *params = reinterpret_cast<const DottedLineParams*>(data);
    if (params->cliprect &&
        !(x >= params->cliprect->left && x < params->cliprect->right &&
          y >= params->cliprect->top && y < params->cliprect->bottom))
    {
        return;
    }
    if (x % 2 == 0 && y % 2 == 0) {
        SetPixel(params->hdc, x, y, params->color);
    }
};
void DottedLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color,
                const RECT* cliprect)
{
    DottedLineParams params = { hdc, color, cliprect };
    LineDDA(x1, y1, x2, y2, &DottedLineCB, (LPARAM)&params);
}


COM_Initialize::~COM_Initialize() {
    if (m_initialized) {
        CoUninitialize();
        m_initialized = false;
    }
}

bool COM_Initialize::Initialize() {
    m_initialized = SUCCEEDED(CoInitialize(NULL));
    return m_initialized;
}

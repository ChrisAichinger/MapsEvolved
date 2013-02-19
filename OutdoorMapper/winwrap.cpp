#include <stdexcept>
#include <cassert>

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

void ImageList::AddIcon(const class IconHandle &icon) {
    ImageList_AddIcon(m_handle, icon.Get());
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


// Listview Stuff
ListViewItem::ListViewItem()
    : m_lvitem(), m_text()
{
    m_lvitem.pszText = const_cast<LPWSTR>(m_text.c_str());
}

ListViewItem::ListViewItem(const LVITEM &lvitem, const std::wstring &text)
    : m_lvitem(lvitem), m_text(text)
{
    m_lvitem.pszText = const_cast<LPWSTR>(m_text.c_str());
}

ListViewItem::ListViewItem(const ListViewItem &rhs)
    : m_lvitem(rhs.m_lvitem), m_text(rhs.m_text)
{
    m_lvitem.pszText = const_cast<LPWSTR>(m_text.c_str());
}


ListViewItem ListViewTextItem(const std::wstring &text) {
    LVITEM lvitem = {0};
    lvitem.mask = LVIF_TEXT | LVIF_STATE;
    return ListViewItem(lvitem, text);
}

ListViewItem ListViewTextImageItem(const std::wstring &text, int image_id) {
    LVITEM lvitem = {0};
    lvitem.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvitem.iImage = image_id;
    return ListViewItem(lvitem, text);
}

void ListViewRow::AddItem(const ListViewItem& item) {
    m_vec.push_back(item);
}


ListViewColumn::ListViewColumn()
    : m_lvcolumn(), m_text()
{
    m_lvcolumn.pszText = const_cast<LPWSTR>(m_text.c_str());
}

ListViewColumn::ListViewColumn(const LVCOLUMN &lvcolumn,
                               const std::wstring &text)
    : m_lvcolumn(lvcolumn), m_text(text)
{
    m_lvcolumn.pszText = const_cast<LPWSTR>(m_text.c_str());
}

ListViewColumn::ListViewColumn(const ListViewColumn &rhs)
    : m_lvcolumn(rhs.m_lvcolumn), m_text(rhs.m_text)
{
    m_lvcolumn.pszText = const_cast<LPWSTR>(m_text.c_str());
}


ListView::~ListView() {
    if (m_hwnd) {
        RemoveWindowSubclass(m_hwnd, &s_SubClassProc, 0);
    }
}

void ListView::Create(HWND hwndParent, const RECT &rect) {
    m_hwnd = CreateWindow(
            WC_LISTVIEW, L"",
            WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            hwndParent, (HMENU)0, g_hinst, NULL);

    if (!m_hwnd)
        throw std::runtime_error("Failed to create list view.");

    if (!SetWindowSubclass(m_hwnd, &s_SubClassProc, 0,
                           reinterpret_cast<DWORD_PTR>(this)))
    {
        DestroyWindow(m_hwnd);
        throw std::runtime_error("Failed to subclass list view.");
    }
}

void ListView::SetImageList(std::unique_ptr<ImageList> &&imagelist) {
    ListView_SetImageList(m_hwnd, imagelist->Get(), LVSIL_SMALL);
    m_imagelist.swap(imagelist);
}

void ListView::InsertColumns(int n_columns, const LVCOLUMN columns[]) {
    for (int iCol = 0; iCol < n_columns; iCol++) {
        m_columns.push_back(
                ListViewColumn(columns[iCol], columns[iCol].pszText));

        ListViewColumn &col = m_columns[m_columns.size() - 1];
        col.m_lvcolumn.iSubItem = iCol;
        if (ListView_InsertColumn(m_hwnd, iCol, &col) == -1)
            throw std::runtime_error("Failed to insert listview column");
    }
}

void ListView::InsertRow(const ListViewRow &row, int desired_index) {
    int subitem_index = 0;
    auto it = row.cbegin();
    assert(it != row.cend());

    LVITEM item;
    item = it->GetLVITEM();
    item.iItem = desired_index;
    item.iSubItem = subitem_index;
    int index = ListView_InsertItem(m_hwnd, &item);
    if (index == -1)
        throw std::runtime_error("Failed to insert listview item");

    ++it;
    ++subitem_index;
    for (; it != row.cend(); ++it, ++subitem_index) {
        item = it->GetLVITEM();
        item.iItem = index;
        item.iSubItem = subitem_index;
        if (!ListView_SetItem(m_hwnd, &item))
            throw std::runtime_error("Failed to insert listview item");
    }
    m_rows.insert(m_rows.begin() + index, row);
}

LRESULT ListView::SubClassProc(HWND hwnd, UINT msg,
                               WPARAM wParam, LPARAM lParam)
{
    if (hwnd != m_hwnd)
        return DefSubclassProc(hwnd, msg, wParam, lParam);

    if (msg == WM_PAINT) {
        RECT rectUpdate, rectRedraw, rectItem;
        GetUpdateRect(hwnd, &rectUpdate, FALSE);

        // allow default processing first
        DefSubclassProc(hwnd, msg, wParam, lParam);

        int idxTop = ListView_GetTopIndex(hwnd);        // First visible row
        int idxItems = ListView_GetCountPerPage(hwnd);  // Number rows visible
        int idxLast = min(idxTop + idxItems, ListView_GetItemCount(hwnd) - 1);

        for (int i = idxTop; i <= idxLast; i++) {
            ListView_GetItemRect(hwnd, i, &rectItem, LVIR_BOUNDS);

            // If item rect intersects update rect then it requires re-drawing
            if (IntersectRect(&rectRedraw, &rectUpdate, &rectItem)) {
                // invalidate the row rectangle then...
                InvalidateRect(hwnd, &rectRedraw, FALSE);

                if (m_rows[i].WantColor()) {
                    // change color and force redraw
                    COLORREF color_orig = ListView_GetTextBkColor(hwnd);
                    ListView_SetTextBkColor(hwnd, m_rows[i].GetColor());
                    DefSubclassProc(hwnd, msg, wParam, lParam);
                    // change back to original color
                    ListView_SetTextBkColor(hwnd, color_orig);
                } else {
                    DefSubclassProc(hwnd, msg, wParam, lParam);
                }

            }
        }
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        DefSubclassProc(hwnd, msg, wParam, lParam);

        int idxTop = ListView_GetTopIndex(hwnd);        // First visible row
        int idxItems = ListView_GetCountPerPage(hwnd);  // Number rows visible
        int idxLast = min(idxTop + idxItems, ListView_GetItemCount(hwnd) - 1);

        for (int i = idxTop; i <= idxLast; i++) {
            if (!m_rows[i].WantColor())
                continue;

            RECT r;
            ListView_GetItemRect(hwnd, i, &r, LVIR_BOUNDS);
            HBRUSH brush = CreateSolidBrush(m_rows[i].GetColor());
            FillRect((HDC)wParam, &r, brush);
            DeleteObject(brush);
        }
        return TRUE;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ListView::s_SubClassProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam,
                                          UINT_PTR uId, DWORD_PTR data)
{
    ListView *plv = reinterpret_cast<ListView*>(data);
    return plv->SubClassProc(hwnd, msg, wParam, lParam);
}

void ListView::RegisterEventHandlers(const ListViewEvents &events) {
    m_events = events;
}

bool ListView::HandleNotify(WPARAM wParam, LPARAM lParam) {
    LPNMHDR pnmhdr = reinterpret_cast<LPNMHDR>(lParam);
    if (pnmhdr->hwndFrom != m_hwnd)
        return false;

    switch (pnmhdr->code) {
    case LVN_ITEMCHANGED:
        if (m_events.ItemChanged)
            m_events.ItemChanged(*this,
                                 reinterpret_cast<LPNMLISTVIEW>(lParam));
        return true;

    case NM_DBLCLK:
        if (m_events.DoubleClick)
            m_events.DoubleClick(*this,
                                 reinterpret_cast<LPNMITEMACTIVATE>(lParam));
        return true;
    }
    return false;
}

RegistryKey::RegistryKey(HKEY hkey)
    : m_key(hkey), m_is_open(hkey != INVALID_HANDLE_VALUE)
{}

RegistryKey::RegistryKey(HKEY hkey, const std::wstring &subkey,
                         enum reg_access access)
    : m_key((HKEY)INVALID_HANDLE_VALUE), m_is_open(false)
{
    REGSAM sam = (access == REG_READ) ? KEY_READ : KEY_WRITE;
    LONG success = RegOpenKeyEx(hkey, subkey.c_str(), 0, sam, &m_key);
    m_is_open = (success == ERROR_SUCCESS);
}

RegistryKey::~RegistryKey() {
    if (m_key != INVALID_HANDLE_VALUE) {
        RegCloseKey(m_key);
        m_key = (HKEY)INVALID_HANDLE_VALUE;
        m_is_open = false;
    }
}

bool RegistryKey::GetStringList(const std::wstring &keyvalue,
                                std::vector<std::wstring> *strings)
{
    if (!m_is_open)
        return false;

    DWORD type, char_len;
    LONG success = RegGetValue(m_key, NULL, keyvalue.c_str(), RRF_RT_ANY,
                               &type, NULL, &char_len);
    if (success != ERROR_SUCCESS)
        return false;
    if (type != REG_MULTI_SZ)
        return false;

    size_t wchar_len = (char_len + sizeof(wchar_t) - 1) / sizeof(wchar_t);
    // zero initialize data - notice the () after [wchar_len]
    auto data = std::unique_ptr<wchar_t[]>(new wchar_t[wchar_len]());
    success = RegGetValue(m_key, NULL, keyvalue.c_str(), RRF_RT_REG_MULTI_SZ,
                          NULL, data.get(), &char_len);

    if (success != ERROR_SUCCESS)
        return false;

    const wchar_t *p = data.get();
    const wchar_t *end = data.get() + wchar_len;
    size_t length = wcsnlen(p, end - p);
    while (length > 0 && p + length < end) {
        strings->push_back(std::wstring(p, length));
        p += length + 1;
        length = wcsnlen(p, end - p);
    }
    return true;
}

bool RegistryKey::SetStringList(const std::wstring &keyvalue,
                                const std::vector<std::wstring> &strings)
{
    if (!m_is_open)
        return false;

    // empty string terminates the string list - allocate space for final \0
    DWORD wchar_len = 1;
    for (auto it = strings.cbegin(); it != strings.cend(); it++) {
        wchar_len += it->length() + 1;  // account for \0
    }

    // zero initialize data - notice the () after [wchar_len]
    auto data = std::unique_ptr<wchar_t[]>(new wchar_t[wchar_len]());

    wchar_t *p = data.get();
    const wchar_t *end = p + wchar_len;
    for (auto it = strings.cbegin(); it != strings.cend(); it++) {
        assert(p < end);
        wcscpy_s(p, end - p, it->c_str());
        p += it->length() + 1;
    }

    LONG res = RegSetValueEx(m_key, keyvalue.c_str(), 0, REG_MULTI_SZ,
                             reinterpret_cast<BYTE*>(data.get()),
                             wchar_len * sizeof(wchar_t));

    return res == ERROR_SUCCESS;
}

bool RegistryKey::GetDWORD(const std::wstring &keyvalue, DWORD *value) {
    if (!m_is_open)
        return false;

    DWORD size = sizeof(*value);
    LONG success = RegGetValue(m_key, NULL, keyvalue.c_str(),
                               RRF_RT_REG_DWORD, NULL, value, &size);
    return success == ERROR_SUCCESS;
}

bool RegistryKey::SetDWORD(const std::wstring &keyvalue, DWORD value) {
    if (!m_is_open)
        return false;

    LONG res = RegSetValueEx(m_key, keyvalue.c_str(), 0, REG_DWORD,
                             reinterpret_cast<BYTE*>(&value),
                             sizeof(value));

    return res == ERROR_SUCCESS;
}

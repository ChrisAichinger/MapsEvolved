#ifndef ODM__WINWRAP_H
#define ODM__WINWRAP_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <list>

#include <Windows.h>
#include <commctrl.h>

#include "util.h"
#include "odm_config.h"

extern HINSTANCE g_hinst;


void DottedLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color,
                const RECT* cliprect = NULL);


class PrinterDC {
    public:
        explicit PrinterDC(HDC hdc) : m_hdc(hdc) {};
        ~PrinterDC() { Cleanup(); };

        void Reset(HDC hdc) { Cleanup(); m_hdc = hdc; };
        HDC Get() const { return m_hdc; };
    private:
        DISALLOW_COPY_AND_ASSIGN(PrinterDC);
        void Cleanup() { if (m_hdc) { DeleteDC(m_hdc); m_hdc = NULL; } };
        HDC m_hdc;
};

class TemporaryWindowDisable {
    public:
        TemporaryWindowDisable(HWND hwnd);
        ~TemporaryWindowDisable();
        void EnableNow();
    private:
        DISALLOW_COPY_AND_ASSIGN(TemporaryWindowDisable);
        HWND m_hwnd;
};

class GlobalMem {
    public:
        explicit GlobalMem(HGLOBAL hglobal) : m_hg(hglobal) {};
        ~GlobalMem() { Cleanup(); };

        void Reset(HGLOBAL hglobal) { Cleanup(); m_hg = hglobal; };
        HGLOBAL Get() const { return m_hg; };
    private:
        DISALLOW_COPY_AND_ASSIGN(GlobalMem);
        void Cleanup() { if (m_hg) { GlobalFree(m_hg); m_hg = NULL; } };
        HGLOBAL m_hg;
};

class EXPORT DevContext {
    public:
        explicit DevContext(HWND hwnd);
        ~DevContext();

        void SetPixelFormat();
        void ForceRepaint();
        HDC Get() const { return m_hdc; };
    private:
        DISALLOW_COPY_AND_ASSIGN(DevContext);
        HWND m_hwnd;
        HDC m_hdc;
};

class EXPORT OGLContext {
    public:
        explicit OGLContext(const std::shared_ptr<DevContext> &device);
        ~OGLContext();
        HGLRC Get() const { return m_hglrc; };
        class DevContext *GetDevContext() { return m_device.get(); };
    private:
        DISALLOW_COPY_AND_ASSIGN(OGLContext);
        HGLRC m_hglrc;

        // We keep the shared_ptr around so the DevContext can't be destroyed
        // before the OGLContext
        const std::shared_ptr<DevContext> m_device;
};

class PaintContext {
    public:
        PaintContext(HWND hwnd);
        ~PaintContext();

        HDC GetHDC() const { return m_hdc; };
        PAINTSTRUCT *GetPS() { return &m_ps; };
    private:
        DISALLOW_COPY_AND_ASSIGN(PaintContext);
        HWND m_hwnd;
        HDC m_hdc;
        PAINTSTRUCT m_ps;
};

class ImageList {
    public:
        ImageList(int width, int height, unsigned int flags, int length);
        ~ImageList();
        HIMAGELIST Get() { return m_handle; };
        void AddIcon(const class IconHandle &icon);
        std::unique_ptr<class IconHandle> GetIcon(int index);
    private:
        DISALLOW_COPY_AND_ASSIGN(ImageList);
        HIMAGELIST m_handle;
};

class IconHandle {
    public:
        IconHandle(HINSTANCE hInstance, LPCTSTR lpIconName);
        IconHandle(HICON hIcon);
        ~IconHandle();
        HICON Get() const { return m_handle; };
    private:
        DISALLOW_COPY_AND_ASSIGN(IconHandle);
        HICON m_handle;
};

class BitmapHandle {
    public:
        BitmapHandle(HINSTANCE hInstance, LPCTSTR lpIconName);
        ~BitmapHandle();
        HBITMAP Get() { return m_handle; };
    private:
        DISALLOW_COPY_AND_ASSIGN(BitmapHandle);
        HBITMAP m_handle;
};

class PenHandle {
    public:
        PenHandle(DWORD dwStyle, DWORD dwWidth, const LOGBRUSH *pBrush);
        ~PenHandle();
        HPEN Get() const { return m_handle; };
    private:
        DISALLOW_COPY_AND_ASSIGN(PenHandle);
        HPEN m_handle;
};


class Sizer {
    public:
        explicit Sizer(HWND parent)
            : m_hwndParent(parent), m_hwndToolbar(0), m_hwndStatusbar(0)
            {}
        void SetHWND(HWND hwnd) { m_hwndParent = hwnd; };
        virtual void SetToolbar(HWND hwndToolbar) {
            m_hwndToolbar = hwndToolbar;
        };
        virtual void SetStatusbar(HWND hwndStatusbar) {
            m_hwndStatusbar = hwndStatusbar;
        };
    protected:
        HWND m_hwndParent;
        HWND m_hwndToolbar;
        HWND m_hwndStatusbar;
        void GetClientRect(RECT &rect);
};


class Window
{
    public:
        HWND GetHWND() { return m_hwnd; }
    protected:
        virtual LRESULT HandleMessage(
                UINT uMsg, WPARAM wParam, LPARAM lParam);
        virtual void PaintContent(PAINTSTRUCT *pps) { };
        virtual LPCTSTR ClassName() = 0;
        virtual UINT WCStyle() { return 0; };
        virtual BOOL WinRegisterClass(WNDCLASS *pwc)
                { return RegisterClass(pwc); }
        virtual ~Window() { }

        HWND WinCreateWindow(DWORD dwExStyle, LPCTSTR pszName,
                DWORD dwStyle, int x, int y, int cx, int cy,
                HWND hwndParent, HMENU hmenu);
    private:
        void Register();
        void OnPaint();
        void OnPrintClient(HDC hdc);
        static LRESULT CALLBACK s_WndProc(HWND hwnd,
                UINT uMsg, WPARAM wParam, LPARAM lParam);
    protected:
        HWND m_hwnd;
};

POINT GetClientMousePos(HWND hwnd);
HWND FindWindowWithinProcess(const std::wstring &wndClass);
std::wstring LoadResString(UINT uID, HINSTANCE hInst = (HINSTANCE)-1);
const wchar_t *LoadResPWCHAR(UINT uID, HINSTANCE hInst = (HINSTANCE)-1);


class ODM_INTERFACE IPrintAbortHandler {
    public:
        virtual bool PrintAbortCallback(int iCode) = 0;
};

class PrintAbortTicket {
    friend class PrintAbortManager;
    private:
        PrintAbortTicket(class PrintAbortManager *pam, HDC hdc);
        std::shared_ptr<class PrintAbortImpl> m_impl;
};

class PrintAbortManager {
    public:
        static PrintAbortManager *Instance();
        PrintAbortTicket RegisterAbort(HDC hdc, IPrintAbortHandler *pdlg);

    private:
        DISALLOW_COPY_AND_ASSIGN(PrintAbortManager);

        PrintAbortManager();
        static BOOL CALLBACK s_AbortProc(HDC hdcPrn, int iCode);
        void ReturnTicket(HDC hdc);

        HDC m_hdc;
        IPrintAbortHandler *m_pdlg;

    friend class PrintAbortImpl;
};


class PrintDialog : public IPrintAbortHandler {
    public:
        PrintDialog(HWND hwnd_parent, TemporaryWindowDisable *twd,
                    const std::wstring &app_name);
        ~PrintDialog();
        void Close();

        // SetAbortProc callback function/message pump
        virtual bool PrintAbortCallback(int iCode);
    private:
        DISALLOW_COPY_AND_ASSIGN(PrintDialog);

        // Dialog callback functions
        BOOL PrintDlgProc(UINT message, WPARAM wParam, LPARAM lParam);
        static BOOL CALLBACK s_PrintDlgProc(HWND hDlg, UINT message,
                                            WPARAM wParam, LPARAM lParam);

        HWND m_hwnd, m_hwnd_parent;
        TemporaryWindowDisable *m_twd;
        bool m_user_abort;
        const std::wstring m_app_name;
};

class ODM_INTERFACE IPrintClient {
    public:
        virtual bool Print(HDC hdc) = 0;
};

struct PrintOrder {
    std::wstring DocName;
    std::wstring PrintDialogName;
    IPrintClient &print_client;
};
bool Print(HWND hwnd_parent, const struct PrintOrder &order);
bool PageSetupDialog(HWND hwnd_parent);

bool FileExists(const wchar_t* fname);


class ListViewItem {
    public:
        ListViewItem();
        ListViewItem(const LVITEM &lvitem, const std::wstring &text);
        ListViewItem(const ListViewItem &rhs);

        friend void swap(ListViewItem& first, ListViewItem& second) {
            using std::swap; // enable ADL
            swap(first.m_lvitem, second.m_lvitem);
            swap(first.m_text, second.m_text);
        }

        ListViewItem& ListViewItem::operator=(ListViewItem rhs) {
            swap(*this, rhs);
            return *this;
        }

        const LVITEM &GetLVITEM() const { return m_lvitem; }
        const std::wstring &GetText() const { return m_text; }
    private:
        LVITEM m_lvitem;
        std::wstring m_text;
};

ListViewItem ListViewTextItem(const std::wstring &text);
ListViewItem ListViewTextImageItem(const std::wstring &text, int image_id);

class ListViewRow {
    public:
        ListViewRow() : m_vec(), m_color(0), m_want_color(false), m_depth(0) {};
        void AddItem(const ListViewItem& item);
        unsigned int GetColor() const { return m_color; }
        bool WantColor() const { return m_want_color; }
        void SetColor(unsigned int color) {
            m_want_color = true;
            m_color = color;
        }

        static const unsigned int MAX_DEPTH = sizeof(unsigned int) * 8 - 1;
        unsigned int GetDepth() const { return m_depth; }
        void SetDepth(unsigned int depth) { m_depth = depth; }

        const ListViewItem &operator[](size_t index) const {
            return m_vec[index];
        }
        std::vector<const ListViewItem>::const_iterator cbegin() const {
            return m_vec.cbegin();
        }
        std::vector<const ListViewItem>::const_iterator cend() const {
            return m_vec.cend();
        }
    private:
        std::vector<const ListViewItem> m_vec;
        unsigned int m_color;
        bool m_want_color;
        unsigned int m_depth;
};

class ListViewColumn {
    public:
        ListViewColumn();
        ListViewColumn(const LVCOLUMN &lvcolumn, const std::wstring &text);
        ListViewColumn(const ListViewColumn &rhs);

        friend void swap(ListViewColumn& first, ListViewColumn& second) {
            using std::swap; // enable ADL
            swap(first.m_lvcolumn, second.m_lvcolumn);
            swap(first.m_text, second.m_text);
        }

        ListViewColumn& ListViewColumn::operator=(ListViewColumn rhs) {
            swap(*this, rhs);
            return *this;
        }

        const LVCOLUMN &GetLVCOLUMN() const { return m_lvcolumn; }
        const std::wstring &GetText() const { return m_text; }
    private:
        LVCOLUMN m_lvcolumn;
        std::wstring m_text;

        friend class ListView;
};

struct EventResult {
    EventResult() : was_handled(false), result(0) {};
    EventResult(bool was_handled_, LRESULT result_)
        : was_handled(was_handled_), result(result_) {};

    bool was_handled;
    LRESULT result;
};


class ControlInterface {
    public:
        virtual EventResult TryHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};


struct ListViewEvents {
    std::function<EventResult(const class ListView& lv, LPNMLISTVIEW pnmlv)> ItemChanged;
    std::function<EventResult(const class ListView& lv, LPNMITEMACTIVATE pnmlv)> LeftClick;
    std::function<EventResult(const class ListView& lv, LPNMITEMACTIVATE pnmlv)> DoubleClick;
    std::function<EventResult(const class ListView& lv, LPNMITEMACTIVATE pnmlv)> RightClick;
    std::function<EventResult(const class ListView& lv, LPNMLVKEYDOWN pnmlv)> KeyDown;
    std::function<EventResult(const class ListView& lv, LPNMHDR pnmlv)> EnterPressed;
};

class ListView : public ControlInterface {
    public:
        ListView()
            : m_hwnd(0), m_imagelist(), m_columns(), m_rows()
            {};
        virtual ~ListView();

        virtual void Create(HWND hwndParent, const RECT &rect);
        virtual void Resize(const RECT &rect);
        virtual void SetImageList(std::unique_ptr<class ImageList> &&imagelist);
        virtual void DeleteAllRows();
        virtual void InsertColumns(int n_columns, const LVCOLUMN columns[]);
        virtual int InsertRow(const ListViewRow &row, int desired_index);
        virtual void RegisterEventHandlers(const ListViewEvents &events);
        virtual EventResult TryHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

        virtual int GetSelectedRow() const;
        virtual HWND GetHWND() const { return m_hwnd; };
    protected:
        HWND m_hwnd;
        std::unique_ptr<class ImageList> m_imagelist;
        std::vector<ListViewColumn> m_columns;
        std::vector<ListViewRow> m_rows;
        ListViewEvents m_events;

        LRESULT SubClassProc(HWND hwnd, UINT msg,
                             WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK s_SubClassProc(HWND hwnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam,
                                               UINT_PTR uId, DWORD_PTR data);

    private:
        DISALLOW_COPY_AND_ASSIGN(ListView);
};

class TreeList : public ListView {
    public:
        TreeList() : ListView(), m_row_tree_index(), m_row_tree_valid(false) {};
        virtual void Create(HWND hwndParent, const RECT &rect);
        virtual int InsertRow(const ListViewRow &row, int desired_index);
        virtual EventResult TryHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    protected:
        void RecalcTreeIndex();
        unsigned int GetTreeIndex(size_t index) {
            if (!m_row_tree_valid) RecalcTreeIndex();
            return m_row_tree_index[index];
        };
        std::vector<unsigned int> m_row_tree_index;
        bool m_row_tree_valid;

    private:
        DISALLOW_COPY_AND_ASSIGN(TreeList);
};

struct ButtonEvents {
    std::function<EventResult(const class Button& btn)> Clicked;
};

class Button : public ControlInterface {
    public:
        Button() : m_hwnd(0) {};
        ~Button();

        void Create(HWND hwndParent, const RECT &rect,
                    const std::wstring &title, int id);
        void RegisterEventHandlers(const ButtonEvents &events);
        virtual EventResult TryHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

        HWND GetHWND() const { return m_hwnd; };
    private:
        HWND m_hwnd;
        ButtonEvents m_events;

        DISALLOW_COPY_AND_ASSIGN(Button);
};

class RegistryKey {
    public:
        enum reg_access { REG_READ, REG_WRITE };

        explicit RegistryKey(HKEY hkey);
        RegistryKey(HKEY hkey, const std::wstring &subkey,
                   enum reg_access access);
        ~RegistryKey();

        bool IsOpen() const { return m_is_open; };

        bool GetStringList(const std::wstring &keyvalue,
                           std::vector<std::wstring> *strings);
        bool SetStringList(const std::wstring &keyvalue,
                           const std::vector<std::wstring> &strings);

        bool GetString(const std::wstring &keyvalue,
                       std::wstring *strings);
        bool SetString(const std::wstring &keyvalue,
                       const std::wstring &strings);

        bool GetDWORD(const std::wstring &keyvalue, DWORD *value);
        bool SetDWORD(const std::wstring &keyvalue, DWORD value);

    private:
        HKEY m_key;
        bool m_is_open;

        DISALLOW_COPY_AND_ASSIGN(RegistryKey);
};

class COM_Initialize {
    public:
        COM_Initialize() : m_initialized(false) {}
        ~COM_Initialize();
        bool Initialize();
    private:
        bool m_initialized;

        DISALLOW_COPY_AND_ASSIGN(COM_Initialize);
};


class ToolbarButton {
    friend class Toolbar;
    public:
        static const int CHECKABLE = (1<<0);
        static const int CHECKED = (1<<1);
        static const int SEPARATOR = (1<<2);
        ToolbarButton(int bitmap_res, int command_id, bool enabled,
                      int options, const std::wstring &alt_text);
    private:
        int m_bitmap_res;
        int m_command_id;
        bool m_enabled;
        int m_options;
        std::wstring m_alt_text;
};

class Toolbar : public ControlInterface {
    public:
        Toolbar(HWND hwndParent, int bitmapSize, int controlID);
        void Resize();
        void SetButtons(const std::list<ToolbarButton> &buttons);
        HWND GetHWND() { return m_hwndTool; };

        virtual EventResult TryHandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    private:
        DISALLOW_COPY_AND_ASSIGN(Toolbar);
        HWND m_hwndParent, m_hwndTool;

        LRESULT SendMsg(UINT Msg, WPARAM wParam, LPARAM lParam);
        LRESULT SendMsg_NotNull(UINT Msg, WPARAM wParam, LPARAM lParam,
                                const char* fail_msg);
        LRESULT AddBitmap(int resource_id, unsigned int num_images);
        void FillButtonStruct(TBBUTTON *button, const ToolbarButton &tbb);

        int m_bitmapSize;
};

#endif

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


class BitmapHandle {
    public:
        BitmapHandle(HINSTANCE hInstance, LPCTSTR lpIconName);
        ~BitmapHandle();
        HBITMAP Get() { return m_handle; };
    private:
        DISALLOW_COPY_AND_ASSIGN(BitmapHandle);
        HBITMAP m_handle;
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

#endif

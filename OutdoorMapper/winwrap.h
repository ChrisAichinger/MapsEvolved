#ifndef ODM__WINWRAP_H
#define ODM__WINWRAP_H

#include <memory>

#include <Windows.h>
#include <commctrl.h>

#include "util.h"

extern HINSTANCE g_hinst;


class DevContext {
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

class OGLContext {
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
    private:
        DISALLOW_COPY_AND_ASSIGN(ImageList);
        HIMAGELIST m_handle;
};

class IconHandle {
    public:
        IconHandle(HINSTANCE hInstance, LPCTSTR lpIconName);
        ~IconHandle();
        HICON Get() { return m_handle; };
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

#endif

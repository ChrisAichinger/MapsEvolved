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


#endif

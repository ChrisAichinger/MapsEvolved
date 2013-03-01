
#include <windows.h>
#include <assert.h>
#include <commctrl.h>

#include "resource.h"
#include "winwrap.h"
#include "win_main.h"
#include "win_mappanel.h"

int PASCAL
WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int nShowCmd) {
    g_hinst = hinst;

    COM_Initialize com_init;
    if (!com_init.Initialize()) {
        MessageBox(0, L"Failed to initialize COM.", L"Outdoormapper Error",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icex)) {
        MessageBox(0, L"Failed to initialize common controls.",
                   L"Outdoormapper Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!RegisterMapWindow()) {
        MessageBox(0, L"Failed to register window classes.",
                   L"Outdoormapper Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RootWindow *prw = RootWindow::Create();
    if (!prw) {
        MessageBox(0, L"Failed to create map window.",
                   L"Outdoormapper Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(prw->GetHWND(), nShowCmd);
    MSG msg;
    BOOL gm_result;
    while ((gm_result = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (gm_result == -1) {
            MessageBox(0, L"Failed to pump messages.",
                       L"Outdoormapper Error", MB_OK | MB_ICONERROR);
            return 1;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // End of lifetime: COM_Initialize
    return msg.wParam;
}

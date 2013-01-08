
#include <sstream>

#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>

#include "resource.h"
#include "winwrap.h"
#include "win_main.h"
#include "win_mappanel.h"


HINSTANCE g_hinst;

#pragma comment(linker,"\"/manifestdependency:type='win32' \
  name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
  processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


int PASCAL
WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int nShowCmd) {
    g_hinst = hinst;

    INITCOMMONCONTROLSEX icex;        // Structure for control initialization.
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    RegisterMapWindow();

    RootWindow *prw = RootWindow::Create();
    if (prw) {
        ShowWindow(prw->GetHWND(), nShowCmd);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

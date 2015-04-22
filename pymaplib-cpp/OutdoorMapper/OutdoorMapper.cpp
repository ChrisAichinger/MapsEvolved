
#include <windows.h>
#include <assert.h>
#include <commctrl.h>

#include "winwrap.h"

#include <stdio.h>

#ifdef ODM_SHARED_LIB
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hinst = hinstDLL;
    }
    return TRUE;
}
#endif

#ifndef ODM__WINMAPPANEL_H
#define ODM__WINMAPPANEL_H

#include <Windows.h>
#include "odm_config.h"

const wchar_t * const g_MapWndClass = L"ODM_MapWindow";

class ODM_INTERFACE IMapWindow {
    public:
        virtual ~IMapWindow() = 0;
	virtual void GetSize(int& w, int& h) = 0;
	virtual void Resize() = 0;
};

IMapWindow *GetIMapWindow(HWND hwnd);
ATOM RegisterMapWindow();


// Extended NMHDR struct to pass additional data via WM_NOTIFY
// The content of data is message specific; cf. enum mwMessages documentation
struct NMHDRExtraData {
    NMHDR nmhdr;
    UINT_PTR data;
};
typedef NMHDRExtraData *PNMHDRExtraData;

// Message codes used to communicate with parent via WM_NOTIFY
// The use of NMHDRExtraData::data is described for each message
// If no description is given, data is not used.
enum mwMessages {
    MW_PAINT = 20,  // Paint map window
    MW_LCLICK,
    MW_DBLCLK,      // Map window doubleclicked
    MW_MWHEEL,      // Mouse wheel
                    // nmhdrextradata.data == wParam of WM_MOUSEWHEEL
    MW_MOUSEMOVE,   // Mouse moved, no drag

    // Drag messages.
    // All of these use nmhdrextradata.data == PMW_DragStruct
    MW_DRAGSTART,   // Mouse drag started
    MW_DRAGEND,     // Mouse drag ended
    MW_DRAG,        // Mouse moved while drag active
};

struct MW_MouseData {
    int xPos, yPos, fwKeys, zDelta;
};

// Structure describing the progress of a mouse drag
struct MW_DragData {
    MW_MouseData start;  // Mouse state when drag was started (MW_DRAGSTART)
    MW_MouseData last;   // Mouse state at the last MW_DRAG message
    MW_MouseData cur;    // Current mouse state
};

#endif

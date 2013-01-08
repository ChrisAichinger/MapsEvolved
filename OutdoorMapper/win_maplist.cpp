
#include "win_maplist.h"
#include "rastermap.h"
#include "projection.h"
#include "resource.h"
#include "win_main.h"

static const wchar_t * const MapListClassname = L"clsODM_Maplist";
LPCTSTR MapListWindow::ClassName() {
    return MapListClassname;
}

MapListWindow *MapListWindow::Create(class RootWindow &rootwindow, 
                                     class RasterMapCollection &maps) {
    MapListWindow *self = new MapListWindow(rootwindow, maps);
    if (self && self->WinCreateWindow(0,
                TEXT("OutdoorMapper Map List"), WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                NULL, NULL)) {
        return self;
    }
    delete self;
    return NULL;
}

MapListWindow::MapListWindow(RootWindow &rootwindow,
                             RasterMapCollection &maps)
    : m_rootwindow(rootwindow), m_maps(maps), m_sizer(0)
{ }

void MapListWindow::PaintContent(PAINTSTRUCT *pps) {
}

LRESULT MapListWindow::HandleMessage(
        UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_CREATE:
            return OnCreate();  
        case WM_NOTIFY:
            LPNMHDR pnmhdr = (LPNMHDR)lParam; 
            if (pnmhdr->hwndFrom == m_hwndListview) {
                switch (pnmhdr->code) {
                case LVN_ITEMCHANGED: {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam; 
                    if (0 == (pnmv->uNewState & LVIS_SELECTED))
                        break;
                    const RasterMap &map = m_maps.Get(pnmv->iItem);
                    std::string char_proj(map.GetProj().GetProjString());
                    std::wstring proj(WStringFromUTF8(char_proj));
                    SetWindowText(m_hwndStatic, proj.c_str());
                    break;
                }
                case NM_DBLCLK: {
                    LPNMITEMACTIVATE pnmv = (LPNMITEMACTIVATE)lParam;
                    m_maps.SetMain(pnmv->iItem);
                    m_rootwindow.ForceRedraw();
                    break;
                }
                }
            }
    }
    return __super::HandleMessage(uMsg, wParam, lParam);
}

LRESULT MapListWindow::OnCreate() {
    RECT rect;
    m_sizer.SetHWND(m_hwnd);

    m_sizer.GetListview(rect);

    // Create the list-view window in report view with label editing enabled.
    m_hwndListview = CreateWindow(
            WC_LISTVIEW, L"",
            WS_CHILD | LVS_REPORT | LVS_SHAREIMAGELISTS,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)0, g_hinst, NULL); 

    if (!m_hwndListview)
        throw std::runtime_error("Failed to create map list view.");

    m_lvImages.reset(new ImageList(GetSystemMetrics(SM_CXSMICON), 
                                   GetSystemMetrics(SM_CYSMICON), 
                                   ILC_MASK | ILC_COLOR32, 10));

    IconHandle icon(g_hinst, MAKEINTRESOURCE(IDI_MAP_STD));
    ImageList_AddIcon(m_lvImages->Get(), icon.Get());
    ListView_SetImageList(m_hwndListview, m_lvImages->Get(), LVSIL_SMALL); 

    // Add the columns.
    for (int iCol = 0; iCol < 1; iCol++)
    {
        LVCOLUMN lvc;

        if ( iCol < 2 )
            lvc.fmt = LVCFMT_LEFT;  // Left-aligned column.
        else
            lvc.fmt = LVCFMT_RIGHT; // Right-aligned column.

        std::wstring col_name = LoadResString(IDS_MLV_COL_FNAME + iCol);
        lvc.pszText = const_cast<LPWSTR>(col_name.c_str());

        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.iSubItem = iCol;
        lvc.cx = 100;               // Width of column in pixels.

        // Insert the columns into the list view.
        if (ListView_InsertColumn(m_hwndListview, iCol, &lvc) == -1)
            throw std::runtime_error("Failed to insert listview column");
    }


    // Initialize LVITEM members that are different for each item.
    for (unsigned int index = 0; index < m_maps.Size(); index++)
    {
        LVITEM lvI;

        // Initialize LVITEM members that are common to all items.
        const std::wstring &fname(m_maps.Get(index).GetFname());
        lvI.pszText   = const_cast<LPWSTR>(fname.c_str());
        lvI.mask      = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
        lvI.stateMask = 0;
        lvI.iSubItem  = 0;
        lvI.state     = 0;
        lvI.iItem  = index;
        lvI.iImage = 0;
    
        // Insert items into the list.
        if (ListView_InsertItem(m_hwndListview, &lvI) == -1)
            throw std::runtime_error("Failed to insert listview item");
    }
    ShowWindow(m_hwndListview, SW_SHOW);

    m_sizer.GetTextbox(rect);
    m_hwndStatic = CreateWindow(
            L"STATIC", L"TEST",
            WS_CHILD | WS_VISIBLE,
            rect.left, rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            m_hwnd, (HMENU)0, g_hinst, NULL);
    return 0;
}

void ShowMapListWindow(class RootWindow &rw, class RasterMapCollection &maps) {
    HWND hwnd = FindWindowWithinProcess(MapListClassname);
    if (!hwnd) {
        hwnd = MapListWindow::Create(rw, maps)->GetHWND();
    }
    if (!SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)) {
        throw std::runtime_error("Failed to activate map manager window.");
    }
}

void MapListSizer::GetListview(RECT &rect) {
    GetClientRect(m_hwndParent, &rect); 
    rect.left += 150;
    rect.bottom /= 2;
}

void MapListSizer::GetTextbox(RECT &rect) {
    GetClientRect(m_hwndParent, &rect); 
    rect.left += 150;
    rect.top += (rect.bottom - rect.top) / 2;
}

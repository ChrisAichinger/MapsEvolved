#include "uimode.h"
#include "win_main.h"
#include "mapdisplay.h"

UIModeNormal::UIModeNormal(RootWindow *rootwindow,
                           std::shared_ptr<MapDisplayManager> &mapdisplay)
    : m_rootwindow(rootwindow), m_mapdisplay(mapdisplay), m_map_drag(false)
{}

void UIModeNormal::OnLClick(const MouseEvent &event) {
    MessageBeep(MB_ICONERROR);
}

void UIModeNormal::OnDblClick(const MouseEvent &event) {
    m_mapdisplay->CenterToDisplayCoord(event.x, event.y);
}

void UIModeNormal::OnMouseWheel(const MouseEvent &event)
{
    int istep = round_to_int(event.wheel_step);
    m_mapdisplay->StepZoom(istep, event.x, event.y);
    m_rootwindow->UpdateStatusbar();
}

void UIModeNormal::OnMapMouseMove(const MouseEvent &event) {
    m_rootwindow->UpdateStatusbar();
}

void UIModeNormal::OnDragStart(const struct DragEvent &dragdata) {
    m_rootwindow->SetCursor(CUR_DRAG_HAND);
}

void UIModeNormal::OnDrag(const struct DragEvent &dragdata) {
    m_mapdisplay->DragMap(dragdata.cur.x - dragdata.last.x,
                          dragdata.cur.y - dragdata.last.y);
}

void UIModeNormal::OnDragEnd(const struct DragEvent &dragdata) {
    m_rootwindow->SetCursor(CUR_NORMAL);
}


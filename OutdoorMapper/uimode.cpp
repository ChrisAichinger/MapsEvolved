#include "uimode.h"
#include "win_main.h"
#include "mapdisplay.h"

UIModeNormal::UIModeNormal(
        RootWindow *rootwindow,
        const std::shared_ptr<MapDisplayManager> &mapdisplay)
    : m_rootwindow(rootwindow), m_mapdisplay(mapdisplay), m_map_drag(false)
{}

void UIModeNormal::OnLClick(const MouseEvent &event) {
    MessageBeep(MB_ICONERROR);
}

void UIModeNormal::OnDblClick(const MouseEvent &event) {
    m_mapdisplay->CenterToDisplayCoord(DisplayCoord(event.x, event.y));
}

void UIModeNormal::OnMouseWheel(const MouseEvent &event) {
    m_mapdisplay->StepZoom(event.wheel_step, DisplayCoord(event.x, event.y));
    m_rootwindow->UpdateStatusbar();
}

void UIModeNormal::OnMapMouseMove(const MouseEvent &event) {
    m_rootwindow->UpdateStatusbar();
}

void UIModeNormal::OnDragStart(const struct DragEvent &dragdata) {
    m_rootwindow->SetCursor(CUR_DRAG_HAND);
}

void UIModeNormal::OnDrag(const struct DragEvent &dragdata) {
    DisplayDelta dd(dragdata.cur.x - dragdata.last.x,
                    dragdata.cur.y - dragdata.last.y);
    m_mapdisplay->DragMap(dd);
}

void UIModeNormal::OnDragEnd(const struct DragEvent &dragdata) {
    m_rootwindow->SetCursor(CUR_NORMAL);
}


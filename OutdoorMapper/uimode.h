#ifndef ODM__UIMODE_H
#define ODM__UIMODE_H

#include <memory>
#include <string>

struct MouseEvent {
    int x, y;
    int flags;
    double wheel_step;
};

struct DragEvent {
    MouseEvent start;
    MouseEvent last;
    MouseEvent cur;
};

class UIMode {
    public:
        virtual std::wstring GetName() const = 0;
        virtual void OnLClick(const MouseEvent &event) {};
        virtual void OnDblClick(const MouseEvent &event) {};
        virtual void OnMouseWheel(const MouseEvent &event) {};
        virtual void OnMapMouseMove(const MouseEvent &event) {};

        virtual void OnDragStart(const DragEvent &dragdata) {};
        virtual void OnDrag(const DragEvent &dragdata) {};
        virtual void OnDragEnd(const DragEvent &dragdata) {};
};

class UIModeNormal : public UIMode {
    public:
        UIModeNormal(
                class RootWindow *rootwindow,
                const std::shared_ptr<class MapDisplayManager> &mapdisplay);

        virtual std::wstring GetName() const { return L"Normal"; };
        virtual void OnLClick(const MouseEvent &event);
        virtual void OnDblClick(const MouseEvent &event);
        virtual void OnMouseWheel(const MouseEvent &event);
        virtual void OnMapMouseMove(const MouseEvent &event);

        virtual void OnDragStart(const DragEvent &dragdata);
        virtual void OnDrag(const DragEvent &dragdata);
        virtual void OnDragEnd(const DragEvent &dragdata);
    private:
        bool m_map_drag;
        class RootWindow *m_rootwindow;
        std::shared_ptr<class MapDisplayManager> m_mapdisplay;
};

#endif

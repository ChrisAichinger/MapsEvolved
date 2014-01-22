# -*- coding: UTF-8 -*-

import sys
import os
import wx
import wx.xrc as xrc

import pymaplib

def get_xrc_path(xrc_fname):
    fdir = os.path.dirname(os.path.realpath(__file__))
    return os.path.join(fdir, xrc_fname)

def get_resources():
    """ This function provides access to the XML resources in this module."""
    if not hasattr(get_resources, 'res'):
        get_resources.res = xrc.XmlResource(get_xrc_path('main.xrc'))
    return get_resources.res


_command_events = {
    wx.EVT_BUTTON, wx.EVT_CHECKBOX, wx.EVT_CHOICE, wx.EVT_COMBOBOX,
    wx.EVT_LISTBOX, wx.EVT_LISTBOX_DCLICK, wx.EVT_CHECKLISTBOX, wx.EVT_MENU,
    wx.EVT_MENU_RANGE, wx.EVT_CONTEXT_MENU, wx.EVT_RADIOBOX,
    wx.EVT_RADIOBUTTON, wx.EVT_SCROLLBAR, wx.EVT_SLIDER, wx.EVT_TEXT,
    wx.EVT_TEXT_ENTER, wx.EVT_TEXT_MAXLEN, wx.EVT_TOGGLEBUTTON, wx.EVT_TOOL,
    wx.EVT_TOOL_RANGE, wx.EVT_TOOL_RCLICKED, wx.EVT_TOOL_RCLICKED_RANGE,
    wx.EVT_TOOL_ENTER, wx.EVT_COMMAND_LEFT_CLICK, wx.EVT_COMMAND_LEFT_DCLICK,
    wx.EVT_COMMAND_RIGHT_CLICK, wx.EVT_COMMAND_SET_FOCUS,
    wx.EVT_COMMAND_KILL_FOCUS, wx.EVT_COMMAND_ENTER,
}

def EVENT(event, id=wx.ID_ANY, id2=wx.ID_ANY, bind_on_parent=False):
    def func(f):
        if not hasattr(f, 'wxevent'):
            f.wxevent = []
        f.wxevent.append((event, id, id2, bind_on_parent))
        return f
    return func

def MapWXEvents(eventhandler):
    for funcname in dir(eventhandler):
        func = getattr(eventhandler, funcname)
        if hasattr(func, 'wxevent'):
            for event, id, id2, bind_on_parent in func.wxevent:
                source = eventhandler.FindWindowById(id)
                if bind_on_parent or not source:
                    # Allow binding on parents only if the event is a
                    # CommandEvent, since normal Events do not propagate
                    # upwards!
                    assert(event in _command_events)
                    eventhandler.Bind(event, func, id=id, id2=id2)
                else:
                    source.Bind(event, func, id=id, id2=id2)


class HtmlWindow(wx.html.HtmlWindow):
    def __init__(self, parent, id, size=(600,400)):
        wx.html.HtmlWindow.__init__(self,parent, id, size=size)
        if "gtk2" in wx.PlatformInfo:
            self.SetStandardFonts()

    def OnLinkClicked(self, link):
        wx.LaunchDefaultBrowser(link.GetHref())

class AboutBox(wx.Dialog):
    def __init__(self, title, content):
        wx.Dialog.__init__(self, None, -1, title,
            style=wx.DEFAULT_DIALOG_STYLE|wx.RESIZE_BORDER|wx.TAB_TRAVERSAL)
        hwin = HtmlWindow(self, -1, size=(400,200))
        hwin.SetPage(content)
        btn = hwin.FindWindowById(wx.ID_OK)
        irep = hwin.GetInternalRepresentation()
        hwin.SetSize((irep.GetWidth()+25, irep.GetHeight()+10))
        self.SetClientSize(hwin.GetSize())
        self.CentreOnParent(wx.BOTH)
        self.SetFocus()


class xrcmainframe(wx.Frame):
    def __init__(self, program):
        wx.Frame.__init__(self)
        self.program = program

        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        if not get_resources().LoadFrame(self, None, "mainframe"):
            raise RuntimeError("Could not load main frame from XRC file")
        # Define variables for the controls, bind event handlers

        MapWXEvents(self)

        self.panel = xrc.XRCCTRL(self, 'skylinepanel')

        self.maplist = pymaplib.RasterMapCollection()
        with pymaplib.DefaultPersistentStore.Read() as ps:
            maps = ps.GetStringList('maps')
        for mapfile in maps:
            pymaplib.LoadMap(self.maplist, mapfile)

        self.ogldisplay = pymaplib.CreateOGLDisplay(self.panel.GetHandle())
        self.mapdisplay = pymaplib.MapDisplayManager(self.ogldisplay,
                                                     self.maplist.Get(0));

        self.dragEnabled = False
        self.dragSuppress = False
        self.dragStartPos = None

    @EVENT(wx.EVT_PAINT, id=xrc.XRCID('skylinepanel'))
    def OnPaint_mappanel(self, evt):
        dc = wx.PaintDC(self.panel)
        self.mapdisplay.Paint()

    @EVENT(wx.EVT_SIZE, id=xrc.XRCID('skylinepanel'))
    def OnSize_mappanel(self, evt):
        self.mapdisplay.Resize(evt.Size.x, evt.Size.y)

    @EVENT(wx.EVT_BUTTON, id=xrc.XRCID('gobutton'))
    def OnGoButton(self, evt):
        print("OnButton_gobutton()")

    @EVENT(wx.EVT_MENU, id=xrc.XRCID('WxInspectorMenuItem'))
    def OnWxInspector(self, evt):
        import wx.lib.inspection
        wx.lib.inspection.InspectionTool().Show()

    @EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitMenuItem'))
    def OnExit(self, evt):
        self.Close()

    @EVENT(wx.EVT_MENU, id=xrc.XRCID('AboutMenuItem'))
    def OnAbout(self, evt):
        aboutText = """<p>Alpine Skyline by Christian Aichinger
        (<a href="mailto:Greek0@gmx.net">Greek0@gmx.net</a>)."""

        versions = {"python_ver": sys.version.split()[0],
                    "wxpy_ver": wx.VERSION_STRING,
                   }
        dlg = AboutBox("About " + self.program, aboutText % versions)
        dlg.ShowModal()
        dlg.Destroy()

    @EVENT(wx.EVT_MOUSEWHEEL, id=xrc.XRCID('skylinepanel'))
    def OnMouseWheel(self, evt):
        if evt.GetWheelAxis() != wx.MOUSE_WHEEL_VERTICAL:
            evt.Skip()
            return
        pos = evt.GetPosition()
        self.mapdisplay.StepZoom(evt.GetWheelRotation() / evt.GetWheelDelta(),
                                 pymaplib.DisplayCoord(pos.x, pos.y))

    @EVENT(wx.EVT_LEFT_DCLICK, id=xrc.XRCID('skylinepanel'))
    def OnLeftDoubleClick(self, evt):
        pos = pymaplib.DisplayCoord(evt.x, evt.y)
        self.mapdisplay.CenterToDisplayCoord(pos)

    @EVENT(wx.EVT_CHAR, id=xrc.XRCID('skylinepanel'))
    def OnChar(self, evt):
        if self.dragEnabled and evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.panel.ReleaseMouse()
            self.OnCaptureChanged(evt)

    @EVENT(wx.EVT_LEFT_DOWN, id=xrc.XRCID('skylinepanel'))
    def OnLeftDown(self, evt):
        evt.Skip()
        self.dragLastPos = evt.GetPosition()
        self.dragSuppress = False

    @EVENT(wx.EVT_LEFT_UP, id=xrc.XRCID('skylinepanel'))
    def OnLeftUp(self, evt):
        evt.Skip()
        if self.dragEnabled:
            self.panel.ReleaseMouse()
            self.OnCaptureChanged(evt)

    @EVENT(wx.EVT_MOTION, id=xrc.XRCID('skylinepanel'))
    def OnMotion(self, evt):
        # Ignore mouse movement if we're not dragging.
        if not evt.Dragging() or not evt.LeftIsDown():
            return

        if not self.dragSuppress and not self.dragEnabled:
            # Begin the drag operation
            self.dragEnabled = True
            self.panel.CaptureMouse()
            self.Cursor = wx.Cursor(wx.CURSOR_HAND)

        if not self.dragEnabled:
            return

        pos = evt.GetPosition()
        delta = pos - self.dragLastPos
        self.dragLastPos = pos
        self.mapdisplay.DragMap(
                pymaplib.DisplayDelta(delta.x,delta.y))
        self.panel.Refresh(eraseBackground=False)

    # In theory:
    # CAPTURE_LOST is called ONLY for to external reasons (e.g. Alt-Tab).
    # CAPTURE_CHANGED is called by ReleaseMouse() and for external reasons.
    #
    # In practice:
    # We must handle this to exit self.dragEnabled mode without missing
    # corner cases. Due to platform differences, it is not entirely clear
    # when LOST/CHANGED are received. So we make OnCaptureChanged
    # idempotent, and use it to handle both events.
    # Furthermore, we call it from every other appropriate place).
    @EVENT(wx.EVT_MOUSE_CAPTURE_LOST, id=xrc.XRCID('skylinepanel'))
    @EVENT(wx.EVT_MOUSE_CAPTURE_CHANGED, id=xrc.XRCID('skylinepanel'))
    def OnCaptureChanged(self, evt):
        self.dragEnabled = False
        self.dragSuppress = True
        self.Cursor = wx.Cursor()


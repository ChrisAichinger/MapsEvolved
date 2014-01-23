import sys
import os

import wx
import wx.xrc as xrc
import wx.adv
from wx.lib.wordwrap import wordwrap

import pymaplib
from mapsevolved import util

class xrcmainframe(wx.Frame):
    def __init__(self, program):
        wx.Frame.__init__(self)
        self.program = program

        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        if not util.get_resources("main").LoadFrame(self, None, "mainframe"):
            raise RuntimeError("Could not load main frame from XRC file.")
        # Define variables for the controls, bind event handlers

        util.MapWxEvents(self)

        self.panel = xrc.XRCCTRL(self, 'skylinepanel')
        self.statusbar = xrc.XRCCTRL(self, 'MainStatusBar')

        self.maplist = pymaplib.RasterMapCollection()
        with pymaplib.DefaultPersistentStore.Read() as ps:
            maps = ps.GetStringList('maps')
        for mapfile in maps:
            pymaplib.LoadMap(self.maplist, mapfile)

        self.ogldisplay = pymaplib.CreateOGLDisplay(self.panel.GetHandle())
        self.mapdisplay = pymaplib.MapDisplayManager(self.ogldisplay,
                                                     self.maplist.Get(0))
        self.heightfinder = pymaplib.HeightFinder(self.maplist)

        self.dragEnabled = False
        self.dragSuppress = False
        self.dragStartPos = None

    @util.EVENT(wx.EVT_PAINT, id=xrc.XRCID('skylinepanel'))
    def OnPaint_mappanel(self, evt):
        dc = wx.PaintDC(self.panel)
        self.mapdisplay.Paint()

    @util.EVENT(wx.EVT_SIZE, id=xrc.XRCID('skylinepanel'))
    def OnSize_mappanel(self, evt):
        self.mapdisplay.Resize(evt.Size.x, evt.Size.y)

    @util.EVENT(wx.EVT_BUTTON, id=xrc.XRCID('gobutton'))
    def OnGoButton(self, evt):
        print("OnButton_gobutton()")

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('WxInspectorMenuItem'))
    def OnWxInspector(self, evt):
        import wx.lib.inspection
        wx.lib.inspection.InspectionTool().Show()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitMenuItem'))
    def OnExit(self, evt):
        self.Close()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('AboutMenuItem'))
    def OnAbout(self, evt):
        info = wx.adv.AboutDialogInfo()
        info.Name = "Maps Evolved"
        info.Version = "0.0.1"
        info.Copyright = "(C) 2012-2013 Christian Aichinger"
        info.Description = wordwrap(
            "A map viewer implementing advanced features.",
            350, wx.ClientDC(self.panel))
        info.WebSite = ("http://greek0.net", "Greek0.net Homepage")
        info.Developers = ["Christian Aichinger <Greek0@gmx.net>"]
        info.License = wordwrap("Still unknown.", 500,
                                wx.ClientDC(self.panel))
        wx.adv.AboutBox(info)

    @util.EVENT(wx.EVT_MOUSEWHEEL, id=xrc.XRCID('skylinepanel'))
    def OnMouseWheel(self, evt):
        if evt.GetWheelAxis() != wx.MOUSE_WHEEL_VERTICAL:
            evt.Skip()
            return
        pos = evt.GetPosition()
        self.mapdisplay.StepZoom(evt.GetWheelRotation() / evt.GetWheelDelta(),
                                 pymaplib.DisplayCoord(pos.x, pos.y))
        self.UpdateStatusbar()

    @util.EVENT(wx.EVT_LEFT_DCLICK, id=xrc.XRCID('skylinepanel'))
    def OnLeftDoubleClick(self, evt):
        pos = pymaplib.DisplayCoord(evt.x, evt.y)
        self.mapdisplay.CenterToDisplayCoord(pos)

    @util.EVENT(wx.EVT_CHAR, id=xrc.XRCID('skylinepanel'))
    def OnChar(self, evt):
        if self.dragEnabled and evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.panel.ReleaseMouse()
            self.OnCaptureChanged(evt)
        else:
            evt.Skip()

    @util.EVENT(wx.EVT_LEFT_DOWN, id=xrc.XRCID('skylinepanel'))
    def OnLeftDown(self, evt):
        evt.Skip()
        self.dragLastPos = evt.GetPosition()
        self.dragSuppress = False

    @util.EVENT(wx.EVT_LEFT_UP, id=xrc.XRCID('skylinepanel'))
    def OnLeftUp(self, evt):
        evt.Skip()
        if self.dragEnabled:
            self.panel.ReleaseMouse()
            self.OnCaptureChanged(evt)

    @util.EVENT(wx.EVT_MOTION, id=xrc.XRCID('skylinepanel'))
    def OnMotion(self, evt):
        # Ignore mouse movement if we're not dragging.
        if evt.Dragging() and evt.LeftIsDown():
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

        else:
            self.UpdateStatusbar()

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
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_LOST, id=xrc.XRCID('skylinepanel'))
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_CHANGED, id=xrc.XRCID('skylinepanel'))
    def OnCaptureChanged(self, evt):
        self.dragEnabled = False
        self.dragSuppress = True
        self.Cursor = wx.Cursor()

    def UpdateStatusbar(self):
        pos = self.panel.ScreenToClient(wx.GetMousePosition())
        display_point = pymaplib.DisplayCoord(pos.x, pos.y)
        base_point = self.mapdisplay.BaseCoordFromDisplay(display_point)

        zoom_percent = self.mapdisplay.GetZoom() * 100
        self.statusbar.SetStatusText("Zoom: %.0f %%" % zoom_percent, i=5)
        ok, mpp = pymaplib.MetersPerPixel(self.mapdisplay.GetBaseMap(),
                                          base_point)
        if ok:
            self.statusbar.SetStatusText("Map: %.1f m/pix" % mpp, i=4)
        else:
            self.statusbar.SetStatusText("Unknown m/pix", i=4)

        ok, ll = self.mapdisplay.GetBaseMap().PixelToLatLon(base_point)
        if not ok:
            self.statusbar.SetStatusText("lat/lon unknown", i=0)
            self.statusbar.SetStatusText("Height unknown", i=1)
            self.statusbar.SetStatusText("Terrain orientation unknown" , i=2)
            self.statusbar.SetStatusText("Steepness unknown", i=3)
            return
        self.statusbar.SetStatusText("lat/lon = %.5f / %.5f" % (ll.lat, ll.lon))

        ok, ti = self.heightfinder.CalcTerrain(ll)
        if not ok:
            self.statusbar.SetStatusText("Height unknown", i=1)
            self.statusbar.SetStatusText("Terrain orientation unknown" , i=2)
            self.statusbar.SetStatusText("Steepness unknown", i=3)
            return

        NESW = pymaplib.CompassPointFromDirection(ti.slope_face_deg)
        self.statusbar.SetStatusText("Height: %.1f m" % ti.height_m, 1)
        self.statusbar.SetStatusText(
                "Orientation: %3s (%.1f°)" % (NESW, ti.slope_face_deg), 2)
        self.statusbar.SetStatusText("Steepness: %.1f°" % ti.steepness_deg, 3)


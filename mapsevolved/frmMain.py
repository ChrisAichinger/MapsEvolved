import sys
import os

import wx
import wx.xrc as xrc
import wx.adv
from wx.lib.wordwrap import wordwrap

import pymaplib
from mapsevolved import frmMapManager, util

def _(s): return s


class MainFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        if not util.get_resources("main").LoadFrame(self, None, "MainFrame"):
            raise RuntimeError("Could not load main frame from XRC file.")

        util.bind_decorator_events(self)

        self.panel = xrc.XRCCTRL(self, 'MapPanel')
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

        self.manage_maps_window = None

    @util.EVENT(wx.EVT_CLOSE, id=xrc.XRCID('MainFrame'))
    def on_close_window(self, evt):
        evt.Skip()
        # Close all other windows to force wx.App to exit
        if self.manage_maps_window:
            self.manage_maps_window.Close()

    @util.EVENT(wx.EVT_PAINT, id=xrc.XRCID('MapPanel'))
    def on_repaint_mappanel(self, evt):
        dc = wx.PaintDC(self.panel)
        self.mapdisplay.Paint()

    @util.EVENT(wx.EVT_SIZE, id=xrc.XRCID('MapPanel'))
    def on_size_mappanel(self, evt):
        self.mapdisplay.Resize(evt.Size.x, evt.Size.y)

    @util.EVENT(wx.EVT_BUTTON, id=xrc.XRCID('GoButton'))
    def on_go_button(self, evt):
        print("on_go_button()")

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ManageMapsMenuItem'))
    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ManageMapsTBButton'))
    def on_manage_maps(self, evt):
        if self.manage_maps_window:
            self.manage_maps_window.Iconize(False)
            self.manage_maps_window.SetFocus()
            self.manage_maps_window.Raise()
            self.manage_maps_window.Show()
        else:
            self.manage_maps_window = frmMapManager.MapManagerFrame(
                    self.maplist, self.mapdisplay)
            self.manage_maps_window.Show()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitMenuItem'))
    def on_exit(self, evt):
        self.Close()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('WxInspectorMenuItem'))
    def on_wx_inspector(self, evt):
        import wx.lib.inspection
        wx.lib.inspection.InspectionTool().Show()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('AboutMenuItem'))
    def on_about(self, evt):
        info = wx.adv.AboutDialogInfo()
        info.Name = "Maps Evolved"
        info.Version = "0.0.1"
        info.Copyright = "(C) 2013-2014 Christian Aichinger"
        info.Description = wordwrap(
            _("A map viewer implementing advanced features."),
            350, wx.ClientDC(self.panel))
        info.WebSite = ("http://greek0.net", _("Greek0.net Homepage"))
        info.Developers = ["Christian Aichinger <Greek0@gmx.net>"]
        info.License = wordwrap(_("Still unknown."), 500,
                                wx.ClientDC(self.panel))
        wx.adv.AboutBox(info)

    @util.EVENT(wx.EVT_MOUSEWHEEL, id=xrc.XRCID('MapPanel'))
    def on_mousewheel(self, evt):
        if evt.GetWheelAxis() != wx.MOUSE_WHEEL_VERTICAL:
            evt.Skip()
            return
        pos = evt.GetPosition()
        self.mapdisplay.StepZoom(evt.GetWheelRotation() / evt.GetWheelDelta(),
                                 pymaplib.DisplayCoord(pos.x, pos.y))
        self.update_statusbar()

    @util.EVENT(wx.EVT_LEFT_DCLICK, id=xrc.XRCID('MapPanel'))
    def on_left_doubleclick(self, evt):
        pos = pymaplib.DisplayCoord(evt.x, evt.y)
        self.mapdisplay.CenterToDisplayCoord(pos)

    @util.EVENT(wx.EVT_CHAR, id=xrc.XRCID('MapPanel'))
    def on_char(self, evt):
        if self.dragEnabled and evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.panel.ReleaseMouse()
            self.on_capture_changed(evt)
        else:
            evt.Skip()

    @util.EVENT(wx.EVT_LEFT_DOWN, id=xrc.XRCID('MapPanel'))
    def on_left_down(self, evt):
        evt.Skip()
        self.dragLastPos = evt.GetPosition()
        self.dragSuppress = False

    @util.EVENT(wx.EVT_LEFT_UP, id=xrc.XRCID('MapPanel'))
    def on_left_up(self, evt):
        evt.Skip()
        if self.dragEnabled:
            self.panel.ReleaseMouse()
            self.on_capture_changed(evt)

    @util.EVENT(wx.EVT_MOTION, id=xrc.XRCID('MapPanel'))
    def on_mouse_motion(self, evt):
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
            self.update_statusbar()

    # Handle mouse capture during dragging operations.
    # In theory:
    # CAPTURE_LOST is called ONLY for to external reasons (e.g. Alt-Tab).
    # CAPTURE_CHANGED is called by ReleaseMouse() and for external reasons.
    # In practice:
    # We must handle this to exit self.dragEnabled mode without missing
    # corner cases. Due to platform differences, it is not entirely clear
    # when LOST/CHANGED are received. So we make on_capture_changed
    # idempotent, and use it to handle both events.
    # Furthermore, we call it directly from every other appropriate place.
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_LOST, id=xrc.XRCID('MapPanel'))
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_CHANGED, id=xrc.XRCID('MapPanel'))
    def on_capture_changed(self, evt):
        self.dragEnabled = False
        self.dragSuppress = True
        self.Cursor = wx.Cursor()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ZoomInTBButton'))
    def on_zoom_in(self, evt):
        self.mapdisplay.StepZoom(+1)
        self.update_statusbar()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ZoomOutTBButton'))
    def on_zoom_out(self, evt):
        self.mapdisplay.StepZoom(-1)
        self.update_statusbar()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ZoomResetTBButton'))
    def on_zoom_reset(self, evt):
        self.mapdisplay.SetZoomOneToOne()
        self.update_statusbar()

    def update_statusbar(self):
        pos = self.panel.ScreenToClient(wx.GetMousePosition())
        display_point = pymaplib.DisplayCoord(pos.x, pos.y)
        base_point = self.mapdisplay.BaseCoordFromDisplay(display_point)

        zoom_percent = self.mapdisplay.GetZoom() * 100
        self.statusbar.SetStatusText(_("Zoom: %.0f %%") % zoom_percent, i=5)
        ok, mpp = pymaplib.MetersPerPixel(self.mapdisplay.GetBaseMap(),
                                          base_point)
        if ok:
            self.statusbar.SetStatusText(_("Map: %.1f m/pix") % mpp, i=4)
        else:
            self.statusbar.SetStatusText(_("Unknown m/pix"), i=4)

        ok, ll = self.mapdisplay.GetBaseMap().PixelToLatLon(base_point)
        if not ok:
            self.statusbar.SetStatusText(_("lat/lon unknown"), i=0)
            self.statusbar.SetStatusText(_("Height unknown"), i=1)
            self.statusbar.SetStatusText(_("Terrain orientation unknown"), i=2)
            self.statusbar.SetStatusText(_("Steepness unknown"), i=3)
            return
        self.statusbar.SetStatusText(
                _("lat/lon = %.5f / %.5f") % (ll.lat, ll.lon), i=0)

        ok, ti = self.heightfinder.CalcTerrain(ll)
        if not ok:
            self.statusbar.SetStatusText(_("Height unknown"), i=1)
            self.statusbar.SetStatusText(_("Terrain orientation unknown"), i=2)
            self.statusbar.SetStatusText(_("Steepness unknown"), i=3)
            return

        NESW = pymaplib.CompassPointFromDirection(ti.slope_face_deg)
        self.statusbar.SetStatusText(_("Height: %.1f m") % ti.height_m, i=1)
        self.statusbar.SetStatusText(
               _("Orientation: %3s (%.1f°)") % (NESW, ti.slope_face_deg), i=2)
        self.statusbar.SetStatusText(
               _("Steepness: %.1f°") % ti.steepness_deg, i=3)


import sys
import os

import wx
import wx.xrc as xrc
import wx.adv
from wx.lib.wordwrap import wordwrap
import gpxpy

import pymaplib
from mapsevolved import frmMapManager, frmPanorama, frmGPSAnalyzer
from mapsevolved import dlgGotoCoord, util

def _(s): return s


class CustomRearrangeList(wx.CheckListBox):
    def __init__(self, *args, **kwargs):
        wx.CheckListBox.__init__(self, *args, **kwargs)

    def swap(self, index1, index2):
        sel = self.Selection
        checked = self.CheckedItems
        clientdata = [self.GetClientData(i) for i in range(self.Count)]
        items = self.Items
        items[index1], items[index2] = items[index2], items[index1]
        self.Items = items
        if sel == index1:
            self.Selection = index2
        elif sel == index2:
            self.Selection = index1
        self.Check(index1, index2 in checked)
        self.Check(index2, index1 in checked)
        for index in checked:
            if index == index1 or index == index2:
                continue
            self.Check(index)
        for i, data in enumerate(clientdata):
            if i == index1:
                self.SetClientData(index2, data)
            elif i == index2:
                self.SetClientData(index1, data)
            else:
                self.SetClientData(i, data)

    def MoveCurrentUp(self):
        self.swap(self.Selection, self.Selection - 1)

    def MoveCurrentDown(self):
        self.swap(self.Selection, self.Selection + 1)


class MainFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, None, "MainFrame"):
            raise RuntimeError("Could not load main frame from XRC file.")

        self.sb_coord_popup = res.LoadMenu("SBCoordPopup")
        if not self.sb_coord_popup:
            raise RuntimeError("Could not load statusbar coordinates " +
                               "popup menu from XRC.")

        ctrl = CustomRearrangeList(self, id=xrc.XRCID('LayerListBox'))
        res.AttachUnknownControl('LayerListBox', ctrl, self)

        self.set_initial_size()

        util.bind_decorator_events(self)

        self.panel = xrc.XRCCTRL(self, 'MapPanel')
        self.statusbar = xrc.XRCCTRL(self, 'MainStatusBar')
        self.layerlistbox = xrc.XRCCTRL(self, 'LayerListBox')
        self.layer_move_up_btn = xrc.XRCCTRL(self, 'LayerMoveUpBtn')
        self.layer_move_down_btn = xrc.XRCCTRL(self, 'LayerMoveDownBtn')
        self.layer_opacity_slider = xrc.XRCCTRL(self, 'OpacitySlider')
        self.layermgr_panel = xrc.XRCCTRL(self, 'LayerMgrPanel')
        self.toolbar = xrc.XRCCTRL(self, 'MainToolbar')

        rastermapcollection = pymaplib.DefaultRasterMapCollection()
        self.filelist = pymaplib.GeoFilesCollection(rastermapcollection)
        with pymaplib.DefaultPersistentStore.Read() as ps:
            self.filelist.retrieve_from(ps)
        self.ogldisplay = pymaplib.CreateOGLDisplay(self.panel.GetHandle())

        self.mapdisplay = pymaplib.MapDisplayManager(
                self.ogldisplay, self.filelist.maplist[0].item)
        self.heightfinder = pymaplib.HeightFinder(rastermapcollection)

        self.drag_enabled = False
        self.drag_suppress = False
        self.drag_last_pos = None
        self.manage_maps_window = None
        self.panorama_window = None
        self.gpstrackanalyzer_window = None
        self.have_shown_layermgr_once = False
        self.special_layers = []

        self.update_layerlist_from_map()
        self.expand_to_fit_sizer()

    @util.EVENT(wx.EVT_CLOSE, id=xrc.XRCID('MainFrame'))
    def on_close_window(self, evt):
        evt.Skip()
        # Close all other windows to force wx.App to exit
        if self.manage_maps_window:
            self.manage_maps_window.Close()
        if self.panorama_window:
            self.panorama_window.Close()
        if self.gpstrackanalyzer_window:
            self.gpstrackanalyzer_window.Close()

    @util.EVENT(wx.EVT_PAINT, id=xrc.XRCID('MapPanel'))
    def on_repaint_mappanel(self, evt):
        dc = wx.PaintDC(self.panel)
        self.mapdisplay.Paint()

    @util.EVENT(wx.EVT_SIZE, id=xrc.XRCID('MapPanel'))
    def on_size_mappanel(self, evt):
        self.mapdisplay.Resize(evt.Size.x, evt.Size.y)

    @util.EVENT(wx.EVT_BUTTON, id=xrc.XRCID('GoButton'))
    def on_go_button(self, evt):
        print("Go pressed")

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ManageMapsMenuItem'))
    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ManageMapsTBButton'))
    def on_manage_maps(self, evt):
        if self.manage_maps_window:
            util.force_show_window(self.manage_maps_window)
        else:
            self.manage_maps_window = frmMapManager.MapManagerFrame(
                    self, self.filelist, self.mapdisplay)
            self.manage_maps_window.Show()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitMenuItem'))
    def on_exit(self, evt):
        self.Close()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ZoomInMenuItem'))
    def on_zoom_in_menu(self, evt):
        self.mapdisplay.StepZoom(+1)
        self.update_statusbar()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ZoomOutMenuItem'))
    def on_zoom_out_menu(self, evt):
        self.mapdisplay.StepZoom(-1)
        self.update_statusbar()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ZoomResetMenuItem'))
    def on_zoom_reset_menu(self, evt):
        self.mapdisplay.SetZoomOneToOne()
        self.update_statusbar()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ToggleGridMenuItem'))
    def on_toggle_gridlines_menu(self, evt):
        if evt.GetInt():  # Item checked
            m = pymaplib.GeoDrawableShPtr(pymaplib.Gridlines())
            o = pymaplib.OverlaySpec(m, True, 1.0)
            self.special_layers.append(o)
        else:
            self.special_layers = [o for o in self.special_layers
                                   if o.Map.GetType() != o.Map.TYPE_GRIDLINES]
        self.update_map_from_layerlist()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('IPdbMenuItem'))
    def on_ipdb(self, evt):
        try:
            import ipdb
        except ImportError:
            import pdb as ipdb
        ipdb.set_trace()

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
        if self.drag_enabled and evt.GetKeyCode() == wx.WXK_ESCAPE:
            self.panel.ReleaseMouse()
            self.on_capture_changed(evt)
        else:
            evt.Skip()

    @util.EVENT(wx.EVT_LEFT_DOWN, id=xrc.XRCID('MapPanel'))
    def on_left_down(self, evt):
        evt.Skip()
        self.drag_last_pos = evt.GetPosition()
        self.drag_suppress = False

    @util.EVENT(wx.EVT_LEFT_UP, id=xrc.XRCID('MapPanel'))
    def on_left_up(self, evt):
        evt.Skip()
        if self.drag_enabled:
            self.panel.ReleaseMouse()
            self.on_capture_changed(evt)

    @util.EVENT(wx.EVT_MOTION, id=xrc.XRCID('MapPanel'))
    def on_mouse_motion(self, evt):
        # Ignore mouse movement if we're not dragging.
        if evt.Dragging() and evt.LeftIsDown():
            if not self.drag_suppress and not self.drag_enabled:
                # Begin the drag operation
                self.drag_enabled = True
                self.panel.CaptureMouse()
                self.Cursor = wx.Cursor(wx.CURSOR_HAND)

            if not self.drag_enabled:
                return

            pos = evt.GetPosition()
            delta = pos - self.drag_last_pos
            self.drag_last_pos = pos
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
    # We must handle this to exit self.drag_enabled mode without missing
    # corner cases. Due to platform differences, it is not entirely clear
    # when LOST/CHANGED are received. So we make on_capture_changed
    # idempotent, and use it to handle both events.
    # Furthermore, we call it directly from every other appropriate place.
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_LOST, id=xrc.XRCID('MapPanel'))
    @util.EVENT(wx.EVT_MOUSE_CAPTURE_CHANGED, id=xrc.XRCID('MapPanel'))
    def on_capture_changed(self, evt):
        self.drag_enabled = False
        self.drag_suppress = True
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

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('ToggleLayerMgrTBButton'))
    def on_toggle_layermgr_btn(self, evt):
        self.show_layermgr(evt.GetInt())

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('PanoramaTBButton'))
    def on_show_panorama(self, evt):
        if self.panorama_window:
            util.force_show_window(self.panorama_window)
        else:
            self.panorama_window = frmPanorama.PanoramaFrame(
                    self, self.heightfinder.GetActiveMap())
            self.panorama_window.Show()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('GPSTrackAnalyzerTBButton'))
    def on_show_gpstrackanalyzer(self, evt):
        for overlay in self.mapdisplay.GetOverlayList():
            maptype = overlay.GetMap().GetType()
            if maptype == pymaplib.GeoDrawable.TYPE_GPSTRACK:
                gpstrack = overlay.GetMap()
                break
        else:
            util.Warn(self, _("No GPS Track currently displayed."))
            return

        self.show_gpstrackanalyzer(gpstrack)

    def show_gpstrackanalyzer(self, gpstrack):
        if self.gpstrackanalyzer_window:
            if gpstrack == self.gpstrackanalyzer_window.track:
                # Analyze the track that is already open: just bring the window
                # to front.
                util.force_show_window(self.gpstrackanalyzer_window)
                return
            else:
                # Analyze new track: Close the old window and create a new one.
                self.gpstrackanalyzer_window.Close()

                # We need to let wx process the destroy events before
                # re-creating the window, otherwise no events are handled in
                # the new child window (possibly an oddity in wxPython?)
                #
                # Waiting for EVT_WINDOW_DESTROY is not enough, unfortunately.
                # The 50 ms timer seems to solve the issue reliably, though.
                self.gpstrack_timer = wx.Timer(self)
                self.gpstrack_timer.Notify = \
                        lambda: self.show_gpstrackanalyzer(gpstrack)
                self.gpstrack_timer.StartOnce(50)
                return

        self.gpstrackanalyzer_window = frmGPSAnalyzer.GPSTrackAnalyzerFrame(
                                            self, gpstrack, self.heightfinder)
        self.gpstrackanalyzer_window.Show()

    @util.EVENT(wx.EVT_SCROLL, id=xrc.XRCID('OpacitySlider'))
    def on_opacity_slider(self, evt):
        sel_index = self.layerlistbox.Selection
        data = self.layerlistbox.GetClientData(sel_index)
        data.Transparency = 1 - evt.Position / 100
        self.update_map_from_layerlist()

    @util.EVENT(wx.EVT_TOOL,  id=xrc.XRCID('GotoCoordTBButton'))
    def on_goto_coord_button(self, evt):
        # Suppress dragging for this function, otherwise we get spurious drag
        # events when the dialog is closed. The exact cause is unknown.
        self.drag_suppress = True
        try:
            res = wx.ID_CANCEL
            dlg = dlgGotoCoord.GotoCoordDialog(self, self.filelist.dblist)
            try:
                res = dlg.ShowModal()
            finally:
                # Destroy() must be called at all costs, otherwise we hang
                # after closing the main frame.
                dlg.Destroy()

            if res == wx.ID_OK:
                self.mapdisplay.SetCenter(pymaplib.LatLon(*dlg.latlon))
                self.panel.Refresh(eraseBackground=False)
        finally:
            self.drag_suppress = False

    @util.EVENT(wx.EVT_RIGHT_UP, id=xrc.XRCID('MainStatusBar'))
    def on_statusbar_rclick(self, evt):
        latlon_rect = self.statusbar.GetFieldRect(0)
        if latlon_rect.Contains(evt.Position):
            self.PopupMenu(self.sb_coord_popup)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('CopyCoordMenuItem'))
    def on_copy_coord(self, evt):
        center = self.mapdisplay.GetCenter()
        ok, ll = self.mapdisplay.GetBaseMap().PixelToLatLon(center)
        if not ok:
            util.Warn(_("Could not retrieve map center position.\n" +
                        "Is this map georeferenced?"))
            return

        if not wx.TheClipboard.Open():
            util.Warn(_("Could not open clipboard for copying.\n"))
            return

        s = _("%.6f,%.6f") % (ll.lat, ll.lon)
        tdo = util.CustomTextDataObject(s)
        try:
            wx.TheClipboard.SetData(tdo)
        finally:
            wx.TheClipboard.Close()

    @util.EVENT(wx.EVT_BUTTON, id=xrc.XRCID('LayerMoveDownBtn'))
    def on_layer_move_down(self, evt):
        self.layerlistbox.MoveCurrentDown()
        self.update_layermgr_ui()
        self.update_map_from_layerlist()

    @util.EVENT(wx.EVT_BUTTON, id=xrc.XRCID('LayerMoveUpBtn'))
    def on_layer_move_up(self, evt):
        self.layerlistbox.MoveCurrentUp()
        self.update_layermgr_ui()
        self.update_map_from_layerlist()

    @util.EVENT(wx.EVT_LISTBOX, id=xrc.XRCID('LayerListBox'))
    def on_layerlistbox_select(self, evt):
        self.update_layermgr_ui()

    @util.EVENT(wx.EVT_CHECKLISTBOX, id=xrc.XRCID('LayerListBox'))
    def on_layer_check(self, evt):
        index = evt.GetInt()
        if index == self.layerlistbox.Count - 1:
            if not self.layerlistbox.IsChecked(index):
                self.layerlistbox.Check(index)
            self.layerlistbox.SetSelection(index)
            # Manually call our select handler, since SetSelection() doesn't
            # invoke it.
            self.on_layerlistbox_select(None)
            return
        data = self.layerlistbox.GetClientData(index)
        data.Enabled = self.layerlistbox.IsChecked(index)
        self.update_map_from_layerlist()
        # Set selection after updating the map, so we call update_layermgr_ui
        # with the new data already.
        self.layerlistbox.SetSelection(index)
        self.on_layerlistbox_select(None)

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
                _("%.6f, %.6f") % (ll.lat, ll.lon), i=0)

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

    def set_basemap(self, rastermap):
        self.mapdisplay.ChangeMap(rastermap)
        self.update_layerlist_from_map()

    def add_overlay(self, rastermap):
        self.mapdisplay.AddOverlayMap(rastermap)
        if not self.have_shown_layermgr_once:
            # Show the layer manager the first time the user adds an overlay.
            # Demonstrate the functionality without bothering the user again if
            # she hides it.
            self.have_shown_layermgr_once = True
            self.toolbar.ToggleTool(xrc.XRCID('ToggleLayerMgrTBButton'), True)
            self.show_layermgr(True)
        self.update_layerlist_from_map()

    def show_layermgr(self, do_show=True):
        sizer = self.GetSizer()
        sizer.Show(self.layermgr_panel, do_show)
        sizer.Layout()
        self.expand_to_fit_sizer()

    def expand_to_fit_sizer(self):
        minsize = self.GetSizer().GetMinSize()
        clientsize = self.GetClientSize()
        clientsize.x = max(minsize.x, clientsize.x)
        clientsize.y = max(minsize.y, clientsize.y)
        self.SetClientSize(clientsize)

    def name_from_map(self, rastermap, is_basemap):
        basename = os.path.basename(rastermap.GetFname())
        rootname, ext = os.path.splitext(basename)
        if is_basemap:
            rootname = '**' + rootname + '**'
        return rootname

    def update_layermgr_ui(self):
        sel_idx = self.layerlistbox.Selection
        self.layer_move_up_btn.Enable(
                self.layerlistbox.Selection != -1 and
                self.layerlistbox.Selection != 0 and
                self.layerlistbox.Selection < self.layerlistbox.Count - 1)
        self.layer_move_down_btn.Enable(
                self.layerlistbox.Selection != -1 and
                self.layerlistbox.Selection < self.layerlistbox.Count - 2)
        self.layer_opacity_slider.Enable(
                self.layerlistbox.Selection != -1 and
                self.layerlistbox.Selection < self.layerlistbox.Count - 1)
        if sel_idx >= self.layerlistbox.Count - 1:
            # base map
            self.layer_opacity_slider.Value = 100
        elif sel_idx < 0:
            self.layer_opacity_slider.Value = 100
        else:
            transp = self.layerlistbox.GetClientData(sel_idx).Transparency
            self.layer_opacity_slider.Value = 100 - transp * 100

    def update_layerlist_from_map(self):
        self.layerlistbox.Clear()
        # We want the top map to be on top (index 0), but ODM draws the layers
        # from 0 to len() - 1. Thus, we reverse the layer list here.
        overlays = reversed(self.mapdisplay.GetOverlayList())
        for overlayspec in overlays:
            if overlayspec.Map in [o.Map for o in self.special_layers]:
                # Do not show our special layers in the overlay list.
                continue
            rastermap = overlayspec.GetMap()
            name = self.name_from_map(rastermap, is_basemap=False)
            idx = self.layerlistbox.Append(name)
            self.layerlistbox.Check(idx, overlayspec.Enabled)
            self.layerlistbox.SetClientData(idx, overlayspec)

        rastermap = self.mapdisplay.GetBaseMap()
        name = self.name_from_map(rastermap, is_basemap=True)
        idx = self.layerlistbox.Append(name)
        self.layerlistbox.Check(idx)
        self.layerlistbox.SetClientData(idx, None)

        self.update_layermgr_ui()
        # Update map so special layers (e.g. gridlines) stay on top.
        self.update_map_from_layerlist()

    def update_map_from_layerlist(self):
        layers = self.special_layers.copy()
        # Count - 1: We disregard the basemap entirely.
        size = self.layerlistbox.Count - 1
        layers.extend(self.layerlistbox.GetClientData(i) for i in range(size))
        self.mapdisplay.SetOverlayList(reversed(layers))

    def set_initial_size(self):
        disp = wx.Display(wx.Display.GetFromWindow(self))
        area = disp.GetClientArea()
        bestsize = self.GetBestSize()
        width = (area.Width + bestsize.Width) // 2
        height = (area.Height + bestsize.Height) // 2
        self.SetSize((width, height))

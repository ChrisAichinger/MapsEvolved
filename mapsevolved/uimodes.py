import datetime

import wx
import wx.xrc as xrc
import wx.lib.newevent
import gpxpy
import gpxpy.gpx

import pymaplib
from mapsevolved import util

def _(s): return s

# An event that is sent to the parent frame, indicating that the current mode
# wants to end voluntarily. try_exit_mode() still has to be called after this
# event is received, though.
ExitModeEvent, EVT_EXIT_MODE = wx.lib.newevent.NewCommandEvent()

class UIMode:
    """Base class for user interface modes

    UI Modes are used to react to mouse and keyboard interaction differently,
    based on the current program state. Usage examples are a GPS track drawing
    mode or a 3D interaction mode.
    """

    def __init__(self, *args, **kwargs):
        pass

    def try_exit_mode(self):
        """Ask if the mode wants to exit

        This is usually based on user input to end the current mode.
        This function may return True (exit mode) or False (cancel exit).
        """
        return True

    def on_mouse_l_up(self, evt):
        pass

    def on_mouse_l_dblclick(self, evt):
        pass

    def on_mouse_r_up(self, evt):
        pass

    def on_mouse_vert_wheel(self, evt):
        pass

    def on_drag_begin(self, evt):
        pass

    def on_drag(self, evt):
        pass

    def on_drag_end(self, evt):
        pass


class BaseUIMode(UIMode):
    def __init__(self, frame, mapviewmodel, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.frame = frame
        self.panel = frame.panel
        self.mapviewmodel = mapviewmodel

    def try_exit_mode(self):
        return False

    def on_mouse_l_dblclick(self, evt):
        pos = pymaplib.DisplayCoord(evt.x, evt.y)
        self.mapviewmodel.SetCenter(pos)

    def on_mouse_vert_wheel(self, evt):
        pos = evt.GetPosition()
        self.mapviewmodel.StepZoom(evt.GetWheelRotation() / evt.GetWheelDelta(),
                                 pymaplib.DisplayCoord(pos.x, pos.y))
        self.frame.update_statusbar()

    def on_drag_begin(self, evt):
        self.Cursor = wx.Cursor(wx.CURSOR_HAND)

    def on_drag(self, evt, last_pos):
        pos_delta = evt.GetPosition() - last_pos
        self.mapviewmodel.MoveCenter(
                pymaplib.DisplayDelta(pos_delta.x, pos_delta.y))
        self.panel.Refresh(eraseBackground=False)

    def on_drag_end(self, evt):
        self.Cursor = wx.Cursor()


class GPSDrawUIMode(BaseUIMode):
    def __init__(self, frame, mapviewmodel, heightfinder, *args, **kwargs):
        super().__init__(frame=frame, mapviewmodel=mapviewmodel,
                         heightfinder=heightfinder, *args, **kwargs)
        self.frame = frame
        self.panel = frame.panel
        self.mapviewmodel = mapviewmodel
        self.heightfinder = heightfinder

        self.gpx = gpxpy.gpx.GPX()
        self.gpx.tracks.append(gpxpy.gpx.GPXTrack())
        self.gpx.tracks[0].segments.append(gpxpy.gpx.GPXTrackSegment())
        data = pymaplib.gpstracks.GPSData(self.gpx)
        drawable = pymaplib.gpstracks.GPSTrack("", data)
        self.drawable = pymaplib.GeoDrawableShPtr(drawable)
        o = pymaplib.OverlaySpec(self.drawable, True, 1.0)
        self.frame.special_layers.append(o)
        self.frame.update_map_from_layerlist()

        self.rclick_popup = self.frame.xrc_res.LoadMenu("GPSDrawPopup")
        if not self.rclick_popup:
            raise RuntimeError("Could not load GPS drawing " +
                               "popup menu from XRC.")

        util.bind_decorator_events(self, wxcontrol=self.frame)

    def try_exit_mode(self):
        # Only ask to save if the user actually created a track.
        if self.drawable.data.all_points:
            dlg = wx.MessageDialog(
                    self.frame,
                    _("Do you want to save the newly created GPS track?"),
                    _("Save GPS Track?"),
                    wx.YES_NO | wx.CANCEL | wx.CANCEL_DEFAULT)
            res = dlg.ShowModal()
            if res == wx.ID_CANCEL:
                return False
            if res == wx.ID_YES:
                if not self._save_new_gpstrack():
                    # The user aborted the save file dialog.
                    return False

        self.frame.special_layers = [o for o in self.frame.special_layers
                                     if o.Map != self.drawable]
        self.frame.update_map_from_layerlist()
        self.drawable = None
        return True

    def on_mouse_l_up(self, evt):
        disp_coord = pymaplib.DisplayCoord(evt.x, evt.y)
        base_coord = pymaplib.BaseCoordFromDisplay(disp_coord,
                                                   self.mapviewmodel)
        ok, ll = self.mapviewmodel.GetBaseMap().PixelToLatLon(base_coord)
        if not ok:
            return True
        ok, ti = self.heightfinder.calc_terrain(ll)
        if not ok:
            return True

        points = self.gpx.tracks[0].segments[0].points
        # One minute per waypoint, starting Jan 1, 1970, midnight. :-)
        time = datetime.datetime.utcfromtimestamp(60 * len(points))
        pt = gpxpy.gpx.GPXTrackPoint(ll.lat, ll.lon, ti.height_m, time)
        segment = points.append(pt)
        self.drawable.data.update_points()
        self.panel.Refresh(eraseBackground=False)
        return True

    def on_mouse_r_up(self, evt):
        self.frame.PopupMenu(self.rclick_popup)

    def _save_new_gpstrack(self):
        saveFileDialog = wx.FileDialog(
                self.frame, "Save GPS Track", "", "",
                "GPS Exchange Files (*.gpx)|*.gpx",
                wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT)
        if saveFileDialog.ShowModal() == wx.ID_CANCEL:
            return False

        # UTF-8 is hardcoded in gpxpy.
        with open(saveFileDialog.GetPath(), 'w', encoding="UTF-8") as f:
            f.write(self.gpx.to_xml())
        return True

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('UndoLastPointMenuItem'))
    def _on_undo_last_point_menu(self, evt):
        self.gpx.tracks[0].segments[0].points.pop()
        self.drawable.data.update_points()
        self.panel.Refresh(eraseBackground=False)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitGPSDrawMenuItem'))
    def _on_exit_mode_menu(self, evt):
        wx.PostEvent(self.frame, ExitModeEvent(self.frame.Id))


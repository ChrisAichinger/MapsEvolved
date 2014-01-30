import os

import wx
import wx.xrc as xrc

import pymaplib
from mapsevolved import util

def _(s): return s


class MapManagerFrame(wx.Frame):
    def __init__(self, parent, maplist, mapdisplay):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, parent, "MapManagerFrame"):
            raise RuntimeError("Could not load map manager frame from XRC.")

        self.menu = res.LoadMenu("MapManagerPopupMenu")
        if not self.menu:
            raise RuntimeError("Could not load map manager frame from XRC.")

        util.bind_decorator_events(self)

        self.parent = parent
        self.maplist = maplist
        self.mapdisplay = mapdisplay
        self.maptreectrl = xrc.XRCCTRL(self, 'MapTreeList')
        self.projstring_tb = xrc.XRCCTRL(self, 'ProjStringTextBox')
        self.popup_list_item = None

        self.insert_maps()

    def insert_row(self, rastermap, parent=None):
        if parent is None:
            parent = self.maptreectrl.RootItem
        fname = rastermap.GetFname()
        basename = os.path.basename(fname)
        dirname = os.path.dirname(os.path.abspath(fname))
        maptype_names = {
            pymaplib.GeoDrawable.TYPE_MAP: _("Map"),
            pymaplib.GeoDrawable.TYPE_DHM: _("DHM"),
            pymaplib.GeoDrawable.TYPE_GRADIENT_MAP: _("Gradient height map"),
            pymaplib.GeoDrawable.TYPE_STEEPNESS_MAP: _("Steepness height map"),
            pymaplib.GeoDrawable.TYPE_LEGEND: _("Legend"),
            pymaplib.GeoDrawable.TYPE_OVERVIEW: _("Overview"),
            pymaplib.GeoDrawable.TYPE_IMAGE: _("Plain image"),
            pymaplib.GeoDrawable.TYPE_GPSTRACK: _("GPS track"),
            pymaplib.GeoDrawable.TYPE_ERROR: _("Error loading map"),
        }
        maptype = maptype_names[rastermap.GetType()]

        item = self.maptreectrl.AppendItem(parent, basename, data=rastermap)
        self.maptreectrl.SetItemText(item, 1, maptype)
        self.maptreectrl.SetItemText(item, 2, dirname)
        if rastermap.GetType() == pymaplib.RasterMap.TYPE_ERROR:
            #lvrow.SetColor(makeRGB(0xdd, 0, 0));
            pass
        return item

    def insert_maps(self):
        for i in range(self.maplist.Size()):
            rastermap = self.maplist.Get(i)
            item = self.insert_row(rastermap)
            for submap in self.maplist.GetAlternateRepresentations(i):
                self.insert_row(submap, item)
            self.maptreectrl.Expand(item)

    @util.EVENT(wx.adv.EVT_TREELIST_SELECTION_CHANGED,
                id=xrc.XRCID('MapTreeList'))
    def on_selection_changed(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            self.projstring_tb.Value = ""
            return
        rastermap = self.maptreectrl.GetItemData(evt.Item)
        if rastermap.GetType() == rastermap.TYPE_ERROR:
            self.projstring_tb.Value = "Failed to open the map."
            return
        projection_bytes = rastermap.GetProj().GetProjString()
        self.projstring_tb.Value = projection_bytes.decode('utf-8',
                                                           errors='replace')

    @util.EVENT(wx.adv.EVT_TREELIST_ITEM_ACTIVATED,
                id=xrc.XRCID('MapTreeList'))
    def on_item_activated(self, evt):
        # User double-clicked the item or pressed enter on it
        self.display_map(evt.Item)

    @util.EVENT(wx.adv.EVT_TREELIST_ITEM_CONTEXT_MENU,
                id=xrc.XRCID('MapTreeList'))
    def on_item_contextmenu(self, evt):
        self.popup_list_item = evt.Item
        self.PopupMenu(self.menu)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('DisplayMenuItem'))
    def on_display_menu(self, evt):
        self.display_map(self.popup_list_item)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('DisplayOverlayMenuItem'))
    def on_overlay_menu(self, evt):
        self.overlay_map(self.popup_list_item)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('RemoveMenuItem'))
    def on_remove_map_menu(self, evt):
        self.delete_map(self.popup_list_item)

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('MapAddTBButton'))
    def on_add_map(self, evt):
        self.add_map()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('MapRemoveTBButton'))
    def on_remove_map(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            util.Warn(self,
                      _("No map selected!\n\n" +
                        "Please select the map you want to remove."))
            return
        self.delete_map(self.maptreectrl.GetSelection())

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('DisplayTBButton'))
    def on_display_tool(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            util.Warn(self,
                      _("No map selected!\n\n" +
                        "Please select the map you want to display."))
            return
        self.display_map(self.maptreectrl.GetSelection())

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('DisplayOverlayTBButton'))
    def on_overlay_tool(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            util.Warn(self,
                      _("No map selected!\n\n" +
                        "Please select the map you want to overlay."))
            return
        self.overlay_map(self.maptreectrl.GetSelection())

    def display_map(self, item):
        rastermap = self.maptreectrl.GetItemData(item)
        self.parent.set_basemap(rastermap)

    def overlay_map(self, item):
        rastermap = self.maptreectrl.GetItemData(item)
        # Let the parent window add the map, so it can update it's overlay list
        # as well.
        self.parent.add_overlay(rastermap)

    def add_map(self):
        openFileDialog = wx.FileDialog(
                self, "Open Rastermap file", "", "",
                "Supported files|*.tif;*.tiff|" +
                "Geotiff Files (*.tif, *.tiff)|*.tif;*.tiff|" +
                "All files (*.*)|*.*",
                wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
        if openFileDialog.ShowModal() == wx.ID_CANCEL:
            return

        pymaplib.LoadMap(self.maplist, openFileDialog.GetPath())
        ps = pymaplib.CreatePersistentStore()
        if not self.maplist.StoreTo(ps):
            util.Warn(self,
                      _("Couldn't save map preferences\n\n" +
                        "Maps Evolved will continue to work, but the " +
                        "map database changes will be lost on exit."))

        self.maptreectrl.DeleteAllItems()
        self.insert_maps()

    def delete_map(self, item):
        if not item:
            util.Warn(self,
                      _("No map selected for removal\n\n" +
                        "Please select the map you want to remove"))
            return
        rastermap = self.maptreectrl.GetItemData(item)
        if not self.maplist.IsToplevelMap(rastermap):
            util.Warn(self,
                      _("Can't delete map view\n\n" +
                        "You can only remove map files, not their views. " +
                        "Try to delete the corresponding (parent) DHM file " +
                        "instead."))
            return
        for i in range(self.maplist.Size()):
            if self.maplist.Get(i) == rastermap:
                break
        else:
            raise RuntimeError("map to delete not found?")

        self.maplist.DeleteMap(i)
        ps = pymaplib.CreatePersistentStore()
        if not self.maplist.StoreTo(ps):
            util.Warn(self,
                      _("Couldn't save map preferences\n\n" +
                        "Maps Evolved will continue to work, but the " +
                        "map database changes will be lost on exit."))

        self.maptreectrl.DeleteAllItems()
        self.insert_maps()


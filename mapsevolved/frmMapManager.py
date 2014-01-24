import os

import wx
import wx.xrc as xrc

import pymaplib
from mapsevolved import util

def _(s): return s


class MapManagerFrame(wx.Frame):
    def __init__(self, maplist, mapdisplay):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, None, "MapManagerFrame"):
            raise RuntimeError("Could not load map manager frame from XRC.")

        util.bind_decorator_events(self)

        self.maplist = maplist
        self.mapdisplay = mapdisplay
        self.maptreectrl = xrc.XRCCTRL(self, 'MapTreeList')
        self.projstring_tb = xrc.XRCCTRL(self, 'ProjStringTextBox')

        self.insert_maps()

    def insert_row(self, rastermap, parent=None):
        if parent is None:
            parent = self.maptreectrl.RootItem
        fname = rastermap.GetFname()
        basename = os.path.basename(fname)
        dirname = os.path.dirname(os.path.abspath(fname))
        maptype_names = {
            pymaplib.RasterMap.TYPE_MAP: _("Map"),
            pymaplib.RasterMap.TYPE_DHM: _("DHM"),
            pymaplib.RasterMap.TYPE_GRADIENT: _("Gradient height map"),
            pymaplib.RasterMap.TYPE_STEEPNESS: _("Steepness height map"),
            pymaplib.RasterMap.TYPE_LEGEND: _("Legend"),
            pymaplib.RasterMap.TYPE_OVERVIEW: _("Overview"),
            pymaplib.RasterMap.TYPE_IMAGE: _("Plain image"),
            pymaplib.RasterMap.TYPE_ERROR: _("Error loading map"),
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
        rastermap = self.maptreectrl.GetItemData(evt.Item)
        self.mapdisplay.ChangeMap(rastermap)

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('MapRemoveTBButton'))
    def on_remove_map(self, evt):
        item = self.maptreectrl.GetSelection()
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
            localptr = self.maplist.Get(i)
            if localptr.get() == rastermap.get():
                print("found")
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


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

        self.maplist = maplist
        self.mapdisplay = mapdisplay
        self.maptreectrl = xrc.XRCCTRL(self, 'MapTreeList')

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

        item = self.maptreectrl.AppendItem(parent, basename)
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


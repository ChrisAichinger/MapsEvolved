import os

import wx
import wx.dataview
import wx.xrc as xrc

import pymaplib
import pymaplib.composite_maps
from mapsevolved import util, config

def _(s): return s


class MapManagerFrame(wx.Frame):
    def __init__(self, parent, filelist, mapdisplay):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, parent, "MapManagerFrame"):
            raise RuntimeError("Could not load map manager frame from XRC.")

        self.map_popup = res.LoadMenu("MapPopup")
        if not self.map_popup:
            raise RuntimeError("Could not load map popup menu from XRC.")
        self.gps_popup = res.LoadMenu("GPSTrackPopup")
        if not self.gps_popup:
            raise RuntimeError("Could not load GPS popup menu from XRC.")
        self.db_popup = res.LoadMenu("POIDBPopup")
        if not self.db_popup:
            raise RuntimeError("Could not load POI DB menu from XRC.")
        self.noitem_popup = res.LoadMenu("NoItemPopup")
        if not self.noitem_popup:
            raise RuntimeError("Could not load 'no item' popup menu from XRC.")

        util.bind_decorator_events(self)

        self.parent = parent
        self.filelist = filelist
        self.mapdisplay = mapdisplay
        self.maptreectrl = xrc.XRCCTRL(self, 'MapTreeList')
        self.projstring_tb = xrc.XRCCTRL(self, 'ProjStringTextBox')
        self.filetype_filter = xrc.XRCCTRL(self, 'FiletypeFilter')
        for i in range(self.filetype_filter.GetCount()):
            self.filetype_filter.SetSelection(i)
        self.popup_item = None

        # For some funky reason XRCed chokes on width -1 columns. Set it here.
        self.maptreectrl.SetColumnWidth(0, -1)
        self.insert_drawables()

    def insert_row(self, container, drawable, parent=None):
        if parent is None:
            parent = self.maptreectrl.RootItem
        maptype_names = {
            pymaplib.GeoDrawable.TYPE_MAP: _("Map"),
            pymaplib.GeoDrawable.TYPE_DHM: _("DHM"),
            pymaplib.GeoDrawable.TYPE_GRADIENT_MAP: _("Gradient height map"),
            pymaplib.GeoDrawable.TYPE_STEEPNESS_MAP: _("Steepness height map"),
            pymaplib.GeoDrawable.TYPE_LEGEND: _("Legend"),
            pymaplib.GeoDrawable.TYPE_OVERVIEW: _("Overview"),
            pymaplib.GeoDrawable.TYPE_IMAGE: _("Plain image"),
            pymaplib.GeoDrawable.TYPE_GPSTRACK: _("GPS track"),
            pymaplib.GeoDrawable.TYPE_POI_DB: _("POI database"),
            pymaplib.GeoDrawable.TYPE_ERROR: _("Error loading map"),
        }
        if drawable:
            maptype = maptype_names[drawable.GetType()]
        else:
            maptype = maptype_names[container.entry_type]

        # TODO: Fix item name for GPX file segments.
        item = self.maptreectrl.AppendItem(parent, container.basename,
                                           data=(container, drawable))
        self.maptreectrl.SetItemText(item, 1, maptype)
        self.maptreectrl.SetItemText(item, 2, container.dirname)
        if container.entry_type == container.TYPE_ERROR:
            #lvrow.SetColor(makeRGB(0xdd, 0, 0));
            pass
        return item

    def insert_drawables(self):
        INDEX_MAPS = 0
        INDEX_GPSTRACKS = 1
        INDEX_POI_DB = 2
        work_list = [(INDEX_MAPS, self.filelist.maplist),
                     (INDEX_GPSTRACKS, self.filelist.gpxlist),
                     (INDEX_POI_DB, self.filelist.dblist),
                    ]
        for filter_idx, lst in work_list:
            if not self.filetype_filter.IsSelected(filter_idx):
                continue
            for container in lst:
                item = self.insert_row(container, container.drawable)
                for view in container.alternate_views:
                    self.insert_row(container, view, item)
                self.maptreectrl.Expand(item)

    @util.EVENT(wx.dataview.EVT_TREELIST_SELECTION_CHANGED,
                id=xrc.XRCID('MapTreeList'))
    def on_selection_changed(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            self.projstring_tb.Value = ""
            return

        container, drawable = self.maptreectrl.GetItemData(evt.Item)
        if container.entry_type == container.TYPE_ERROR:
            self.projstring_tb.Value = "Failed to open the map."
            return
        if not drawable:
            self.projstring_tb.Value = ""
            return

        proj_bytes = drawable.GetProj().GetProjString()
        self.projstring_tb.Value = proj_bytes.decode('utf-8', errors='replace')

    @util.EVENT(wx.dataview.EVT_TREELIST_ITEM_ACTIVATED,
                id=xrc.XRCID('MapTreeList'))
    def on_item_activated(self, evt):
        # User double-clicked the item or pressed enter on it.
        container, drawable = self.maptreectrl.GetItemData(evt.Item)
        if not drawable:
            # If the item is not actually drawable, abort.
            return
        if drawable.GetType() == pymaplib.GeoDrawable.TYPE_GPSTRACK:
            self.overlay_map(evt.Item)
        else:
            self.display_map(evt.Item)

    @util.EVENT(wx.dataview.EVT_TREELIST_ITEM_CONTEXT_MENU,
                id=xrc.XRCID('MapTreeList'))
    def on_item_contextmenu(self, evt):
        self.popup_item = evt.Item
        itemdata = self.maptreectrl.GetItemData(evt.Item)
        if not itemdata:
            self.PopupMenu(self.noitem_popup)
            return
        container, drawable = itemdata
        if container.entry_type == container.TYPE_GPSTRACK:
            self.PopupMenu(self.gps_popup)
        elif container.entry_type == container.TYPE_POI_DB:
            self.PopupMenu(self.db_popup)
        else:
            self.PopupMenu(self.map_popup)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('DisplayMenuItem'))
    def on_display_menu(self, evt):
        self.display_map(self.popup_item)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('DisplayOverlayMenuItem'))
    def on_overlay_menu(self, evt):
        self.overlay_map(self.popup_item)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('GPSTrackAnalyzerMenuItem'))
    def on_gpsanalyzer_menu(self, evt):
        container, drawable = self.maptreectrl.GetItemData(self.popup_item)
        self.parent.show_gpstrackanalyzer(drawable)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('RemoveMenuItem'))
    def on_remove_map_menu(self, evt):
        self.delete(self.popup_item)

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('AddMapMenuItem'))
    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('MapAddTBButton'))
    def on_add_map(self, evt):
        self.add_map()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('AddGPXMenuItem'))
    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('GPXAddTBButton'))
    def on_add_gpx(self, evt):
        self.add_gpx()

    @util.EVENT(wx.EVT_MENU, id=xrc.XRCID('AddDBMenuItem'))
    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('DBAddTBButton'))
    def on_add_db(self, evt):
        self.add_db()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('MapRemoveTBButton'))
    def on_remove_map(self, evt):
        if not self.maptreectrl.GetSelection().IsOk():
            util.Warn(self,
                      _("No map selected!\n\n" +
                        "Please select the map you want to remove."))
            return
        self.delete(self.maptreectrl.GetSelection())

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

    @util.EVENT(wx.EVT_LISTBOX, id=xrc.XRCID('FiletypeFilter'))
    def on_filetype_filter_select(self, evt):
        self.maptreectrl.DeleteAllItems()
        self.insert_drawables()

    @util.EVENT(wx.EVT_TOOL, id=xrc.XRCID('CreateCompositeTBButton'))
    def on_link_maps(self, evt):
        maplist = self.filelist.maplist
        found_any_composites = False
        for comp in pymaplib.composite_maps.find_composites(maplist):
            found_any_composites = True
            names = (_("{0.basename}   \t   ({0.dirname})").format(c)
                     for c in comp.tile_containers)
            go_ahead = util.YesNo(
                    self,
                    _("Do you want to join the following maps:\n\n" +
                      "{0}\n\n" +
                      "This will remove all of the above maps replace " +
                      "them with one composite map."
                     ).format('\n'.join(sorted(names))))

            if go_ahead:
                fname = comp.get_composite_map_fname()
                self.filelist.add_file(fname, ftype='MAP')
                for c in comp.tile_containers:
                    self.filelist.delete(c)

        if not found_any_composites:
            util.Info(self,
                      _("No compositable maps were found.\n\n" +
                        "To create composite maps, first add the " +
                        "individual map pieces to the map list, then click " +
                        "'Create composite maps'."))


        self.finish_list_change()

    def display_map(self, item):
        container, drawable = self.maptreectrl.GetItemData(item)
        self.parent.set_basemap(drawable)

    def overlay_map(self, item):
        container, drawable = self.maptreectrl.GetItemData(item)
        # Let the parent window add the map, so it can update it's overlay list
        # as well.
        self.parent.add_overlay(drawable)

    def add_map(self):
        openFileDialog = wx.FileDialog(
                self, "Open Rastermap file", "", "",
                "Supported files|*.tif;*.tiff;*.gvg|" +
                "Geotiff files (*.tif, *.tiff)|*.tif;*.tiff|" +
                "Alpine club map files (*.gvg)|*.gvg;|" +
                "All files (*.*)|*.*",
                wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
        if openFileDialog.ShowModal() == wx.ID_CANCEL:
            return

        self.filelist.add_file(openFileDialog.GetPath(), ftype='MAP')
        self.finish_list_change()

    def add_gpx(self):
        openFileDialog = wx.FileDialog(
                self, "Open GPX file", "", "",
                "Supported files|*.gpx;|" +
                "GPX files (*.gpx)|*.gpx|" +
                "All files (*.*)|*.*",
                wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
        if openFileDialog.ShowModal() == wx.ID_CANCEL:
            return

        self.filelist.add_file(openFileDialog.GetPath(), ftype='GPX')
        self.finish_list_change()

    def add_db(self):
        openFileDialog = wx.FileDialog(
                self, "Open POI DB file", "", "",
                "Supported files|*.db;|" +
                "POI DB files (*.db)|*.db|" +
                "All files (*.*)|*.*",
                wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
        if openFileDialog.ShowModal() == wx.ID_CANCEL:
            return

        self.filelist.add_file(openFileDialog.GetPath(), ftype='DB')
        self.finish_list_change()

    def delete(self, item):
        if not item:
            util.Warn(self,
                      _("No map selected for removal\n\n" +
                        "Please select the map you want to remove."))
            return
        container, drawable = self.maptreectrl.GetItemData(item)
        if drawable != container.drawable:
            # Trying to delete an alternative view.
            # Notify the user we're deleting the whole file.
            force = util.YesNo(self,
                               _("Are you sure you want to remove the " +
                                 "following map (including all its views):" +
                                 "\n{}").format(container.basename))
            if not force:
                return
        self.filelist.delete(container)
        self.finish_list_change()

    def finish_list_change(self):
        with config.Config.write() as conf:
            try:
                self.filelist.store_to(conf)
            except KeyError:
                util.Warn(self,
                          _("Couldn't save map preferences\n\n" +
                            "Maps Evolved will continue to work, but the " +
                            "map database changes will be lost on exit."))

        self.maptreectrl.DeleteAllItems()
        self.insert_drawables()


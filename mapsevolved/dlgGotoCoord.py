import os

# Import both wx main module and the xrc module
import wx
import wx.xrc as xrc

import pymaplib
from mapsevolved import util

def _(s): return s


class GotoCoordDialog(wx.Dialog):
    def __init__(self, parent, dblist):
        wx.Dialog.__init__(self)
        # 3-argument LoadDialog() calls self.Create(), skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadDialog(self, parent, "GotoCoordDialog"):
            raise RuntimeError("Could not load dialog from XRC.")

        self.dblist = dblist
        self.latlon = None
        self.data = {}
        self.have_autosized = False

        self.inputtext = xrc.XRCCTRL(self, 'InputText')
        self.results = xrc.XRCCTRL(self, 'ResultList')

        self.results.InsertColumn(0, _("Name"))
        self.results.InsertColumn(1, _("Category"))
        self.results.SetColumnWidth(0, wx.LIST_AUTOSIZE_USEHEADER)
        self.results.SetColumnWidth(1, wx.LIST_AUTOSIZE_USEHEADER)

        # Create Ok and Cancel buttons and insert them before the final spacer.
        # Otherwise there is no border on the bottom side of the dialog.
        btn_idx = self.Sizer.ItemCount - 1
        self.Sizer.Insert(btn_idx, self.CreateButtonSizer(wx.OK | wx.CANCEL))
        self.Sizer.Fit(self)

        util.bind_decorator_events(self)
        self.inputtext.SetFocus()

    def Validate(self):
        if self.latlon:
            return True

        self.latlon = self.data.get(self.results.GetFirstSelected(), None)
        if self.latlon:
            return True

        # Allow successful exit if we only have one viable item left in our
        # search list.
        data_list = [v for v in self.data.values() if v]
        if len(data_list) == 1:
            self.latlon = data_list[0]
            return True

        self.latlon = pymaplib.parse_coordinate(self.inputtext.Value)
        if self.latlon:
            return True
        return False

    def TransferDataFromWindow(self):
        return bool(self.latlon)

    @util.EVENT(wx.EVT_LISTBOX_DCLICK, id=xrc.XRCID('ResultListBox'))
    @util.EVENT(wx.EVT_LIST_ITEM_ACTIVATED, id=xrc.XRCID('ResultList'))
    def on_results_dclick(self, evt):
        item_idx = evt.GetIndex()
        self.latlon = self.data.get(item_idx, None)
        if self.Validate() and self.TransferDataFromWindow():
            self.EndModal(wx.ID_OK)

    @util.EVENT(wx.EVT_TEXT_ENTER,  id=xrc.XRCID('InputText'))
    def on_inputtext_enter(self, evt):
        if self.Validate() and self.TransferDataFromWindow():
            self.EndModal(wx.ID_OK)

    @util.EVENT(wx.EVT_TEXT,  id=xrc.XRCID('InputText'))
    def on_inputtext_change(self, evt):
        self.data = {}
        self.results.DeleteAllItems()

        latlon = pymaplib.parse_coordinate(self.inputtext.Value)
        if latlon:
            self.append_result(_("Coordinate matches:"), "", bold=True)
            self.append_result(_("  {:0.06f}, {:0.06f}").format(*latlon),
                               _("Coordinate"), latlon)
            self.autosize_once()
            # Don't query the DB if the input is in coordinate format.
            # It seems unlikely that a plain coordinate string should occur as
            # a POI in our databases.
            return

        if len(self.inputtext.Value) < 3:
            # Don't query the DB for very short strings.
            return

        for db in self.dblist:
            header_drawn = False
            for match in db.search_name(self.inputtext.Value):
                if not header_drawn:
                    header_drawn = True
                    fname = db.basename
                    self.append_result(_("Matches in '{}':").format(fname), "",
                                       bold=True)
                self.append_result("  {}".format(match.name), match.category,
                                   (match.lat, match.lon))
        self.autosize_once()

    def append_result(self, col1, col2, data=None, bold=False):
        self.results.Append((col1, col2))
        index = self.results.ItemCount - 1
        self.data[index] = data
        if bold:
            font = self.results.GetFont()
            font.MakeBold()
            self.results.SetItemFont(index, font)

    def autosize_once(self):
        if not self.have_autosized:
            self.results.SetColumnWidth(0, wx.LIST_AUTOSIZE)
            self.results.SetColumnWidth(1, wx.LIST_AUTOSIZE)
            self.have_autosized = True

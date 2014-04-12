import wx
import wx.xrc as xrc

import pymaplib
from mapsevolved import util

def _(s): return s


class PanoramaFrame(wx.Frame):
    def __init__(self, parent, dhm):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, parent, "PanoramaFrame"):
            raise RuntimeError("Could not load panorama frame from XRC.")

        util.bind_decorator_events(self)

        self.parent = parent
        self.dhm = dhm
        self.panorama = xrc.XRCCTRL(self, 'PanoramaBitmap')
        self.CalcPanorama(None)

    @util.EVENT(wx.EVT_SIZE, id=xrc.XRCID('PanoramaBitmap'))
    def on_panorama_resize(self, evt):
        evt.Skip()
        size = self.panorama.GetClientRect()
        img = self.bmp.ConvertToImage().Rescale(size.Width, size.Height, wx.IMAGE_QUALITY_BILINEAR)
        self.panorama.SetBitmap(img.ConvertToBitmap())

    def CalcPanorama(self, coord):
        region = pymaplib.CalcPanorama(self.dhm, pymaplib.LatLon(48.22473, 15.15650))
        data = region.GetData()
        #image = wx.Image(region.GetWidth(), region.GetHeight())
        #image.SetData(rgbdata)
        #bmp = wx.Bitmap(image)
        self.bmp = wx.Bitmap(region.GetWidth(), region.GetHeight(), 32)
        self.bmp.CopyFromBuffer(bytes(data), wx.BitmapBufferFormat_RGB32)
        self.bmp = self.bmp.ConvertToImage().Mirror(False).ConvertToBitmap()
        size = self.panorama.GetClientRect()
        img = self.bmp.ConvertToImage().Rescale(size.Width, size.Height)
        self.panorama.SetBitmap(img.ConvertToBitmap())


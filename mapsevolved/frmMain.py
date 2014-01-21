# -*- coding: UTF-8 -*-

import sys
import os
import wx
import wx.xrc as xrc

import pymaplib

def get_xrc_path(xrc_fname):
    fdir = os.path.dirname(os.path.realpath(__file__))
    return os.path.join(fdir, xrc_fname)

def get_resources():
    """ This function provides access to the XML resources in this module."""
    if not hasattr(get_resources, 'res'):
        get_resources.res = xrc.XmlResource(get_xrc_path('main.xrc'))
    return get_resources.res


def EVENT(event, source=None, id=wx.ID_ANY, id2=wx.ID_ANY):
    def func(f):
        f.wxevent = (event, source, id, id2)
        return f
    return func

def MapWXEvents(eventhandler):
    for funcname in dir(eventhandler):
        func = getattr(eventhandler, funcname)
        if hasattr(func, 'wxevent'):
            event, source, id, id2 = func.wxevent
            eventhandler.Bind(event, func, source, id, id2)


class HtmlWindow(wx.html.HtmlWindow):
    def __init__(self, parent, id, size=(600,400)):
        wx.html.HtmlWindow.__init__(self,parent, id, size=size)
        if "gtk2" in wx.PlatformInfo:
            self.SetStandardFonts()

    def OnLinkClicked(self, link):
        wx.LaunchDefaultBrowser(link.GetHref())

class AboutBox(wx.Dialog):
    def __init__(self, title, content):
        wx.Dialog.__init__(self, None, -1, title,
            style=wx.DEFAULT_DIALOG_STYLE|wx.RESIZE_BORDER|wx.TAB_TRAVERSAL)
        hwin = HtmlWindow(self, -1, size=(400,200))
        hwin.SetPage(content)
        btn = hwin.FindWindowById(wx.ID_OK)
        irep = hwin.GetInternalRepresentation()
        hwin.SetSize((irep.GetWidth()+25, irep.GetHeight()+10))
        self.SetClientSize(hwin.GetSize())
        self.CentreOnParent(wx.BOTH)
        self.SetFocus()


class xrcmainframe(wx.Frame):
    def PreCreate(self):
        """ This function is called during the class's initialization.

        Override it for custom setup before the window is created usually to
        set additional window styles using SetWindowStyle() and SetExtraStyle().
        """
        print("xrcmainframe.PreCreate()")
        pass

    def __init__(self, program):
        wx.Frame.__init__(self)
        self.program = program
        self.PreCreate()

        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        if not get_resources().LoadFrame(self, None, "mainframe"):
            raise RuntimeError("Could not load main frame from XRC file")
        # Define variables for the controls, bind event handlers

        self.panel = xrc.XRCCTRL(self, 'skylinepanel')
        MapWXEvents(self)

    @EVENT(wx.EVT_PAINT, id=xrc.XRCID('skylinepanel'))
    def OnPaint_skylinepanel(self, evt):
        print("OnPaint_skylinepanel()")

    @EVENT(wx.EVT_BUTTON, id=xrc.XRCID('gobutton'))
    def OnGoButton(self, evt):
        print("OnButton_gobutton()")

    @EVENT(wx.EVT_MENU, id=xrc.XRCID('ExitMenuItem'))
    def OnExit(self, evt):
        self.Close()

    @EVENT(wx.EVT_MENU, id=xrc.XRCID('AboutMenuItem'))
    def OnAbout(self, evt):
        aboutText = """<p>Alpine Skyline by Christian Aichinger
        (<a href="mailto:Greek0@gmx.net">Greek0@gmx.net</a>)."""

        versions = {"python_ver": sys.version.split()[0],
                    "wxpy_ver": wx.VERSION_STRING,
                   }
        dlg = AboutBox("About " + self.program, aboutText % versions)
        dlg.ShowModal()
        dlg.Destroy()

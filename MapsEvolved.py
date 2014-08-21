#!/usr/bin/python
#-------------------------------------------------------------------------------
#   Maps Evolved
#   Copyright 2013-2014, Christian Aichinger <Greek0@gmx.net>
#-------------------------------------------------------------------------------

import sys
import wx

from mapsevolved import frmMain


def main():
    # Do not redirect errors to popup window.
    app = wx.App(redirect=False)

    # TODO: Make Windows not fiddle with our toolbar buttons.
    # Need to rework our images to have proper alpha channels before, though.
    # Cf. http://wxpython.org/Phoenix/docs/html/ToolBar.html
    #if wx.GetApp().GetComCtl32Version() >= 600 and wx.DisplayDepth() >= 32:
    #    wx.SystemOptions.SetOption("msw.remap", 2)

    top = frmMain.MainFrame()
    app.SetTopWindow(top)
    top.Show()
    app.MainLoop()


if __name__ == '__main__':
    main()

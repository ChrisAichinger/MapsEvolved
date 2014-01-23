#!/usr/bin/python
#-------------------------------------------------------------------------------
#   Maps Evolved
#   Copyright 2013-2014, Christian Aichinger <Greek0@gmx.net>
#-------------------------------------------------------------------------------

import sys
import wx

from mapsevolved import frmMain


def main():
    app = wx.App(redirect=False)   # Do not redirect errors to popup window.
    top = frmMain.MainFrame()
    top.Show()
    app.MainLoop()


if __name__ == '__main__':
    main()

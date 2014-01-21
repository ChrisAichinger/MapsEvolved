#!/usr/bin/python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
#   Alpine Skyline
#-------------------------------------------------------------------------------

import sys
import wx
import wx.html
from mapsevolved import frmMain

PROGRAM = "Alpine Skyline"

class Frame(wx.Frame):
    def __init__(self, title):
        wx.Frame.__init__(self, None, title=title, pos=(150,150), size=(350,200))
        self.statusbar = self.CreateStatusBar()

        panel.Layout()


def main():
    app = wx.App(redirect=False)   # Error messages go to popup window
    top = frmMain.xrcmainframe(PROGRAM)
    top.Show()
    app.MainLoop()


if __name__ == '__main__':
    main()

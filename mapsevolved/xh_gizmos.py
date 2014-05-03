# Name:         xh_gizmos.py
# From:         wxPython-src-3.0.0.0/wxPython/wx/tools/XRCed
# Purpose:      XML handlers for wx.gismos classes
# Author:       Roman Rolinsky <rolinsky@femagsoft.com>
# Created:      09.07.2007
#
# Copyright (c) 2002, Roman Rolinsky <rollrom@users.sourceforge.net>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import wx
import wx.xrc as xrc
import wx.adv
import wx.xml

class EditableListBoxXmlHandler(xrc.XmlResourceHandler):
    def __init__(self):
        xrc.XmlResourceHandler.__init__(self)
        # Standard styles
        self.AddWindowStyles()
        # Custom styles
        self.AddStyle('wxEL_ALLOW_NEW', wx.adv.EL_ALLOW_NEW)
        self.AddStyle('wxEL_ALLOW_EDIT', wx.adv.EL_ALLOW_EDIT)
        self.AddStyle('wxEL_ALLOW_DELETE', wx.adv.EL_ALLOW_DELETE)

    def CanHandle(self, node):
        return self.IsOfClass(node, 'EditableListBox')
#        return self.IsOfClass(node, 'EditableListBox') or \
#               self.insideBox and node.GetName() == 'item'

    # Process XML parameters and create the object
    def DoCreateResource(self):
        assert self.GetInstance() is None

        w = wx.adv.EditableListBox(self.GetParentAsWindow(),
                                   self.GetID(),
                                   self.GetText("label"),
                                   self.GetPosition(),
                                   self.GetSize(),
                                   self.GetStyle(),
                                   self.GetName())

        # Doesn't work
        #self.insideBox = True
        #self.CreateChildrenPrivately(None, self.GetParamNode('content'))
        #self.insideBox = False

        # Long way
        strings = []
        n = self.GetParamNode('content')
        if n: n = n.GetChildren()
        while n:
            if n.GetType() != xrc.XML_ELEMENT_NODE or n.GetName() != "item":
                n = n.GetNext()
                continue
            strings.append(n.GetNodeContent())
            n = n.GetNext()
        w.SetStrings(strings)
        self.SetupWindow(w)
        return w


class TreeListCtrlXmlHandler(xrc.XmlResourceHandler):
    def __init__(self):
        xrc.XmlResourceHandler.__init__(self)
        # Standard styles
        self.AddWindowStyles()
        # Custom styles
        self.AddStyle('wxTR_DEFAULT_STYLE', wx.TR_DEFAULT_STYLE)
        self.AddStyle('wxTR_EDIT_LABELS', wx.TR_EDIT_LABELS)
        self.AddStyle('wxTR_NO_BUTTONS', wx.TR_NO_BUTTONS)
        self.AddStyle('wxTR_HAS_BUTTONS', wx.TR_HAS_BUTTONS)
        self.AddStyle('wxTR_TWIST_BUTTONS', wx.TR_TWIST_BUTTONS)
        self.AddStyle('wxTR_NO_LINES', wx.TR_NO_LINES)
        self.AddStyle('wxTR_FULL_ROW_HIGHLIGHT', wx.TR_FULL_ROW_HIGHLIGHT)
        self.AddStyle('wxTR_LINES_AT_ROOT', wx.TR_LINES_AT_ROOT)
        self.AddStyle('wxTR_HIDE_ROOT', wx.TR_HIDE_ROOT)
        self.AddStyle('wxTR_ROW_LINES', wx.TR_ROW_LINES)
        self.AddStyle('wxTR_HAS_VARIABLE_ROW_HEIGHT',
                      wx.TR_HAS_VARIABLE_ROW_HEIGHT)
        self.AddStyle('wxTR_SINGLE', wx.TR_SINGLE)
        self.AddStyle('wxTR_MULTIPLE', wx.TR_MULTIPLE)
        #self.AddStyle('wxTR_EXTENDED', wx.TR_EXTENDED)

    def CanHandle(self, node):
        return self.IsOfClass(node, 'TreeListCtrl')

    # Process XML parameters and create the object
    def DoCreateResource(self):
        assert self.GetInstance() is None

        w = wx.dataview.TreeListCtrl(self.GetParentAsWindow(),
                                     self.GetID(),
                                     style=self.GetStyle(),
                                     name=self.GetName())
        n = self.GetParamNode('content')
        if n: n = n.GetChildren()
        while n:
            if n.Type != wx.xml.XML_ELEMENT_NODE or n.GetName() != "item":
                n = n.GetNext()
                continue
            try:
                w.AddColumn(n.GetNodeContent())
            except AttributeError:
                w.AppendColumn(n.GetNodeContent())
            n = n.GetNext()
        widths = self.GetText('widths').strip()
        if widths:
            for i, width in enumerate(widths.split(',')):
                w.SetColumnWidth(i, int(width.strip()))
        return w


class DynamicSashWindowXmlHandler(xrc.XmlResourceHandler):
    def __init__(self):
        xrc.XmlResourceHandler.__init__(self)
        # Standard styles
        self.AddWindowStyles()
        # Custom styles
        self.AddStyle('wxDS_MANAGE_SCROLLBARS', wx.adv.DS_MANAGE_SCROLLBARS)
        self.AddStyle('wxDS_DRAG_CORNER', wx.adv.DS_DRAG_CORNER)

    def CanHandle(self, node):
        return self.IsOfClass(node, 'DynamicSashWindow')

    # Process XML parameters and create the object
    def DoCreateResource(self):
        assert self.GetInstance() is None

        w = wx.adv.DynamicSashWindow(self.GetParentAsWindow(),
                                     self.GetID(),
                                     self.GetPosition(),
                                     self.GetSize(),
                                     self.GetStyle(),
                                     self.GetName())

        self.SetupWindow(w)
        return w


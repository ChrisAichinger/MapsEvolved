import os

import wx
import wx.xrc as xrc

from mapsevolved import xh_gizmos

def get_xrc_path_default(xrc_name):
    """"""
    if not xrc_name.endswith('.xrc'):
        xrc_name = xrc_name + '.xrc'
    this_file_dir = os.path.dirname(os.path.realpath(__file__))
    return os.path.join(this_file_dir, xrc_name)

def get_resources(xrc_name, get_xrc_path=get_xrc_path_default):
    """ This function provides access to the XML resources in this module."""
    if not hasattr(get_resources, 'res'):
        get_resources.res = dict()
    if xrc_name not in get_resources.res:
        res = xrc.XmlResource(get_xrc_path(xrc_name))
        res.AddHandler(xh_gizmos.TreeListCtrlXmlHandler())
        get_resources.res[xrc_name] = res
    return get_resources.res[xrc_name]


_command_events = {
    wx.EVT_BUTTON, wx.EVT_CHECKBOX, wx.EVT_CHOICE, wx.EVT_COMBOBOX,
    wx.EVT_LISTBOX, wx.EVT_LISTBOX_DCLICK, wx.EVT_CHECKLISTBOX, wx.EVT_MENU,
    wx.EVT_MENU_RANGE, wx.EVT_CONTEXT_MENU, wx.EVT_RADIOBOX,
    wx.EVT_RADIOBUTTON, wx.EVT_SCROLLBAR, wx.EVT_SLIDER, wx.EVT_TEXT,
    wx.EVT_TEXT_ENTER, wx.EVT_TEXT_MAXLEN, wx.EVT_TOGGLEBUTTON, wx.EVT_TOOL,
    wx.EVT_TOOL_RANGE, wx.EVT_TOOL_RCLICKED, wx.EVT_TOOL_RCLICKED_RANGE,
    wx.EVT_TOOL_ENTER, wx.EVT_COMMAND_LEFT_CLICK, wx.EVT_COMMAND_LEFT_DCLICK,
    wx.EVT_COMMAND_RIGHT_CLICK, wx.EVT_COMMAND_SET_FOCUS,
    wx.EVT_COMMAND_KILL_FOCUS, wx.EVT_COMMAND_ENTER,
}

def EVENT(event, id=wx.ID_ANY, id2=wx.ID_ANY, bind_on_parent=False):
    def func(f):
        if not hasattr(f, 'wxevent'):
            f.wxevent = []
        f.wxevent.append((event, id, id2, bind_on_parent))
        return f
    return func

def bind_decorator_events(eventhandler):
    for funcname in dir(eventhandler):
        func = getattr(eventhandler, funcname)
        if hasattr(func, 'wxevent'):
            for event, id, id2, bind_on_parent in func.wxevent:
                source = eventhandler.FindWindowById(id)
                if bind_on_parent or not source:
                    # Allow binding on parents only if the event is a
                    # CommandEvent, since normal Events do not propagate
                    # upwards!
                    assert(event in _command_events)
                    eventhandler.Bind(event, func, id=id, id2=id2)
                else:
                    source.Bind(event, func, id=id, id2=id2)

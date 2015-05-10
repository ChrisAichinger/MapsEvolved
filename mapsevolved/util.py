# Copyright 2015 Christian Aichinger <Greek0@gmx.net>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import os
import imp
import math
import functools
import contextlib
import collections

import wx
import wx.xrc as xrc
import wx.lib.pubsub

from . import xh_gizmos

def main_is_frozen():
    """Determine if run via py2exe or a similar tool"""
    return (hasattr(sys, "frozen") or    # new py2exe
            hasattr(sys, "importers") or # old py2exe
            imp.is_frozen("__main__"))   # tools/freeze

def get_mapsevolved_dir():
    """Get the mapsevolved package dir

    When run normally, returns the directory of the mapsevolved package.
    When run frozen (i.e. via py2exe or similar), return the directory of the
    main executable.
    """

    if main_is_frozen():
        return os.path.join(os.path.dirname(sys.executable), 'mapsevolved')
    return os.path.dirname(os.path.realpath(__file__))

def get_xrc_path_default(xrc_name):
    """Resolve the name of an XRC file to a full path"""
    if not xrc_name.endswith('.xrc'):
        xrc_name = xrc_name + '.xrc'
    return os.path.join(get_mapsevolved_dir(), xrc_name)

@functools.lru_cache()
def get_resources(xrc_name, get_xrc_path=get_xrc_path_default):
    """ This function provides access to the XML resources in this module."""
    res = xrc.XmlResource(get_xrc_path(xrc_name))
    res.AddHandler(xh_gizmos.TreeListCtrlXmlHandler())
    return res


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

def register_command_event(event_type):
    """Register `event_type` as command event with the EVENT mechanism.

    For non-command events, only the control that caused the event to occur is
    searched for event handlers. In contrast, command events are passed up to
    the parent object if the control doesn't define a handler itself.

    The EVENT mechanism allows binding events on parents only if it knows
    a specific event to be a command event. Doing otherwise would be a pitfall
    for users.

    The list of command events is currently hardcoded. This call can be used
    to extend this list at runtime to register events unknown to the EVENT
    framework.
    """

    _command_events.add(evt_type)

class EventSpec:
    """Internal data store for the EVENT mechanism."""
    def __init__(self, event_type, id, id2, bind_on_parent):
        self.event_type = event_type
        self.id = id
        self.id2 = id2
        self.bind_on_parent = bind_on_parent

def EVENT(event_type, id=wx.ID_ANY, id2=wx.ID_ANY, bind_on_parent=False):
    """Decorator to make the decorated function an event handler.

    Designate the decorated function as wx handler for `event_type` events.
    The event binding does not occur immediately. Instead, the binding is
    created when `bind_decorator_events()` is called during initialization of
    a wx control.

    Both, the `id` and `id2` parameters are forwarded to `Bind()`. If
    `bind_on_parent` is set to `True`, the event is bound on the control's
    parent instead of on the control itself. This is only valid for
    command events, because non-command events don't percolate upwards
    to the parent at all.
    """

    def func(f):
        if not hasattr(f, 'wxevent'):
            f.wxevent = []
        f.wxevent.append(EventSpec(event_type, id, id2, bind_on_parent))
        return f
    return func

def _hook_function(func, pre_func_hook=None, post_func_hook=None):
    """Wrap a function with pre- and post-processing hooks.

    Return a new function that calls `pre_func_hook()`, `func()`,
    `post_func_hook()`.

    All functions receive the same arguments as the wrapper was called with.
    The return value of the wrapper is the return value of `func()`.

    Both hooks are optional and can be omitted.
    """
    if not pre_func_hook and not post_func_hook:
        return func

    @functools.wraps(func)
    def f(*args, **kwargs):
        if pre_func_hook:
            pre_func_hook(*args, **kwargs)
        result = func(*args, **kwargs)
        if post_func_hook:
            post_func_hook(*args, **kwargs)
        return result

    return f

def bind_decorator_events(handlerobj, wxcontrol=None, *,
                          pre_event_hook=None, post_event_hook=None):
    """Bind event handlers registered with util.EVENT

    Bind all event handler functions declared in `handlerobj` with the
    util.EVENT decorator to receive events from `wxcontrol`.

    Arguments:

    * `handlerobj` is an object that contains the `util.EVENT` handler
      function definitions.
    * `wxcontrol` is the wx control to receive events from. If not specified,
      events are bound to `handlerobj` instead. This entails that `handlerobj`
      must be a valid wx control itself, in this case.

    Keyword-only arguments:

    * `pre_event_hook` is a callback to be invoked every time before any event
      handler is called.
    * `post_event_hook` is a callback to be invoked every time after any event
      handler is called.

    Both `pre_event_hook` and `post_event_hook` receive same arguments as the
    handler: a single event parameter. Their return value is ignored.
    """

    if wxcontrol is None:
        wxcontrol = handlerobj

    for funcname in dir(handlerobj):
        func = getattr(handlerobj, funcname)
        func = _hook_function(func, pre_event_hook, post_event_hook)
        if not hasattr(func, 'wxevent'):
            continue
        for spec in func.wxevent:
            source = wxcontrol.FindWindowById(spec.id)
            if spec.bind_on_parent or not source:
                # Allow binding on parents only if the event is a
                # CommandEvent, since normal Events do not propagate
                # upwards!
                assert spec.event_type in _command_events
                wxcontrol.Bind(spec.event_type, func, id=spec.id, id2=spec.id2)
            else:
                source.Bind(spec.event_type, func, id=spec.id, id2=spec.id2)

def PUBSUB(topicName):
    def func(f):
        if not hasattr(f, 'wxpubsub'):
            f.wxpubsub = []
        f.wxpubsub.append(topicName)
        return f
    return func

def bind_decorator_pubsubs(eventhandler):
    for funcname in dir(eventhandler):
        func = getattr(eventhandler, funcname)
        if hasattr(func, 'wxpubsub'):
            for topic_name in func.wxpubsub:
                wx.lib.pubsub.pub.subscribe(func, topic_name)



def YesNo(parent, question, caption='Maps Evolved'):
    """Display a yes/no dialog popup window

    The function returns ``True`` if the user chose 'yes', otherwise ``False``.
    """

    dlg = wx.MessageDialog(parent, question, caption,
                           wx.YES_NO | wx.ICON_QUESTION)
    result = dlg.ShowModal() == wx.ID_YES
    dlg.Destroy()
    return result

def Info(parent, message, caption='Maps Evolved'):
    """Display a info popup window"""

    dlg = wx.MessageDialog(parent, message, caption,
                           wx.OK | wx.ICON_INFORMATION)
    dlg.ShowModal()
    dlg.Destroy()

def Warn(parent, message, caption='Maps Evolved'):
    """Display a warning popup window"""

    dlg = wx.MessageDialog(parent, message, caption, wx.OK | wx.ICON_WARNING)
    dlg.ShowModal()
    dlg.Destroy()

def Error(parent, message, caption='Maps Evolved'):
    """Display an error popup window"""

    dlg = wx.MessageDialog(parent, message, caption, wx.OK | wx.ICON_ERROR)
    dlg.ShowModal()
    dlg.Destroy()

def force_show_window(win):
    """Make a window visible and show it on top

    Try to make ``win`` visible and on top of other windows. This is not 100%
    foolproof, but should work in most circumstances.
    """
    win.Iconize(False)
    win.SetFocus()
    win.Raise()
    win.Show()

@contextlib.contextmanager
def Bitmap_MemoryDC_Context(bmp):
    """Context manager for manipulating wx.Bitmap using wx.MemoryDC

    wx.Bitmap instances can be manipulated by creating a wx.MemoryDC for them.
    Care has to be taken to select the bitmap out of the DC at the end.
    This context manager ensures this.
    """

    mdc = wx.MemoryDC()
    try:
        mdc.SelectObject(bmp)
        yield mdc
    finally:
        mdc.SelectObject(wx.NullBitmap)

def draw_text(dc, text, pos, alignment):
    """Draw text on a DC with alignment

    Draw ``text`` on ``dc`` at ``pos`` with the desired ``alignment``.
    ``pos`` is a position from the topleft of the dc.
    ``alignment`` uses the ``wx.ALIGN_*`` `constants.

    ``pos`` and ``alignment`` act together to set the position of the text:

     - If ``alignment`` == wx.ALIGN_TOP|wx.ALIGN_LEFT:
       The point ``pos`` will be in the top-left of the output text.
     - If ``alignment`` == wx.ALIGN_BOTTOM|wx.ALIGN_RIGHT:
       The point ``pos`` will be in the bottom-right of the output text.
     - And so forth for other combinations...
    """

    text_rect = wx.Rect(0, 0, dc.Size.width, dc.Size.height)
    if alignment & wx.ALIGN_RIGHT:
        text_rect.width = pos.x
    elif alignment & wx.ALIGN_CENTER_HORIZONTAL:
        text_rect.x = pos.x - dc.Size.width
        text_rect.width = 2 * dc.Size.width
    else:
        # Handle ALIGN_LEFT (== 0) in the else clause.
        text_rect.x = pos.x

    if alignment & wx.ALIGN_BOTTOM:
        text_rect.height = pos.y
    elif alignment & wx.ALIGN_CENTER_VERTICAL:
        text_rect.y = pos.y - dc.Size.height
        text_rect.height = 2 * dc.Size.height
    else:
        # Handle ALIGN_TOP (== 0) in the else clause.
        text_rect.y = pos.y

    dc.DrawLabel(text, wx.Bitmap(), text_rect,
                 alignment=alignment)

def round_up_reasonably(value):
    """Round up to a 1, 2 or 5 followed by zeroes

    ``value`` must be a positive number to round.

    If the result is smaller than 1, a ``float`` is returned, otherwise
    the return value is an ``int``.

    >>> round_up_reasonably(1.2)
    2
    >>> round_up_reasonably(21)
    50
    >>> round_up_reasonably(520000)
    1000000
    >>> round_up_reasonably(0.008)
    0.01
    >>> round_up_reasonably(-1.2)
    Traceback (most recent call last):
    ...
    ValueError: math domain error
    """

    exponent = int(math.log10(value))
    mantissa = value / 10**exponent
    if mantissa < 2:
        return 2 * 10**exponent
    elif mantissa < 5:
        return 5 * 10**exponent
    else:
        return 10 * 10**exponent

def round_up_to_multiple(n, unit):
    """Round ``n`` up to a multiple of ``unit``

    Round ``n`` (a positive or negative number) away from zero, so that
    ``result % unit == 0``. In other words, the return value will be an exact
    multiple of ``unit``.

    >>> round_up_to_multiple(14, 4)
    16
    >>> round_up_to_multiple(-14, 4)
    -16
    """

    if n > 0:
        return math.ceil(n / unit) * unit;
    elif  n < 0:
        return math.floor(n / unit) * unit;
    else:
        return unit;

class CustomTextDataObject(wx.DataObjectSimple):
    """Reimplementation of wx.TextDataObject

    wx.TextDataObject is currently (2014.05.03) bugged in Phoenix.
    Reimplement it until a fix is available.

    Cf. http://trac.wxwidgets.org/ticket/15488
        https://groups.google.com/d/topic/wxpython-dev/Isw1L5_i6po/discussion
    """

    def __init__(self, txt=''):
        super().__init__()
        self.SetFormat(wx.DataFormat(wx.DF_TEXT))
        self.SetText(txt)

    def GetDataSize(self):
        return len(self.value)

    def GetDataHere(self, buf):
        buf[:] = self.value
        return True

    def SetData(self, buf):
        self.value = buf.tobytes()
        return True

    def GetText(self):
        return str(self.value, 'utf8')

    def SetText(self, txt):
        self.value = bytes(txt + ' ', 'utf8')

def walk_treectrl_items(tree, root_item=None):
    if root_item is None:
        root_item = tree.RootItem
    if not root_item.IsOk():
        return
    yield root_item

    item, cookie = tree.GetFirstChild(root_item)
    while item.IsOk():
        yield from walk_treectrl_items(tree, item)
        item, cookie = tree.GetNextChild(root_item, cookie)

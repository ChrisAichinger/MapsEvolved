import os
import math
import time
import textwrap

import wx
import wx.xrc as xrc

import gpxpy
import gpxpy.gpx

import pymaplib
from mapsevolved import util

def _(s): return s


class GPSPointStatistics:
    def __init__(self, point_list):
        self.num_points = 0
        self.ele_gps_min = None
        self.ele_gps_max = None
        self.ele_dhm_min = None
        self.ele_dhm_max = None

        self.time_secs_min = None
        self.time_secs_max = None
        self.point_distances = [0]
        self.cumulative_point_distances = [0]

        prev_point = None
        for point in point_list:
            self.num_points += 1
            self.ele_gps_min = self.min(self.ele_gps_min, point.elevation)
            self.ele_gps_max = self.max(self.ele_gps_max, point.elevation)
            self.ele_dhm_min = self.min(self.ele_dhm_min, point.dhm_elevation)
            self.ele_dhm_max = self.max(self.ele_dhm_max, point.dhm_elevation)
            self.time_secs_min = self.min(self.time_secs_min,
                                          point.secs_after_start)
            self.time_secs_max = self.max(self.time_secs_max,
                                          point.secs_after_start)

            if prev_point is not None:
                distance = point.distance_2d(prev_point)
                self.point_distances.append(distance)
                self.cumulative_point_distances.append(
                        self.cumulative_point_distances[-1] + distance)
            prev_point = point

        self.total_distance = self.cumulative_point_distances[-1]
        self.ele_gps_delta = self.ele_gps_max - self.ele_gps_min
        self.ele_dhm_delta = self.ele_dhm_max - self.ele_dhm_min
        self.time_secs_elapsed = self.time_secs_max - self.time_secs_min

    def min(self, result, value):
        if result is None:
            return value
        return min(result, value)

    def max(self, result, value):
        if result is None:
            return value
        return max(result, value)

    def sum(self, result, value):
        if result is None:
            return value
        return result + value


class XAxis_NPoints:
    axis_name = 'N'
    def __init__(self, points, point_stats):
        self.val_min = 0
        self.val_max = point_stats.npoints

    def get_x(self, point_num, point):
        return point_num

    def stringify(self, val):
        return "{}".format(int(round(val)))

    def get_suggested_subticks(self, max_ticks):
        tick_distance = util.round_up_reasonably(self.val_max / max_ticks)
        if tick_distance == 0:
            return []
        return list(range(tick_distance, self.val_max, tick_distance))

class XAxis_Time:
    def __init__(self, points, point_stats):
        self.val_min = point_stats.time_secs_min
        self.val_max = point_stats.time_secs_max

        delta = self.val_max - self.val_min
        if delta > 3600:  # > one hour
            self.axis_name = 't [h]'
            self.display = 'h'
        elif delta > 60:
            self.axis_name = 't [m]'
            self.display = 'm'
        else:
            self.axis_name = 't [s]'
            self.display = 's'

    def get_x(self, point_num, point):
        return point.secs_after_start

    def stringify(self, val):
        if self.display == 's':
            return "{}s".format(val)

        m, s = divmod(val, 60)
        if self.display == 'm':
            if s == 0:
                return "{}m".format(m)
            elif m == 0:
                return "{}s".format(s)
            else:
                return "{}m{}s".format(m, s)

        h, m = divmod(m, 60)
        if self.display == 'h':
            if m == 0:
                return "{}h".format(h)
            elif h == 0:
                return "{}m".format(m)
            else:
                return "{}h{}m".format(h, m)

        raise NotImplementedError('Unknown XAxis_Time.display value: %s',
                                  self.display)

    def get_suggested_subticks(self, max_ticks):
        tick_distance = (self.val_max - self.val_min) / max_ticks
        if tick_distance == 0:
            return []
        m, s = divmod(tick_distance, 60)
        h, m = divmod(m, 60)
        if h:
            tick_distance = util.round_up_reasonably(h) * 3600
        elif m:
            tick_distance = util.round_up_reasonably(m) * 60
        else:
            tick_distance = util.round_up_reasonably(s)
        tick_min = self.val_min + tick_distance
        return list(range(tick_min, self.val_max, tick_distance))

class XAxis_Distance:
    def __init__(self, points, point_stats):
        self._point_stats = point_stats
        self.val_min = 0
        self.val_max = point_stats.total_distance

        if self.val_max <= 1000:
            self.axis_name = 'd [m]'
            self.factor = 1
        else:
            self.axis_name = 'd [km]'
            self.factor = 1000

    def stringify(self, val):
        fmt = "{:.0f}" if self.factor == 1 else "{:.1f}"
        return fmt.format(val / self.factor)

    def get_suggested_subticks(self, max_ticks):
        if self.val_max == 0:
            return []
        tick_distance = util.round_up_reasonably(self.val_max / max_ticks)
        return list(range(tick_distance, self.val_max, tick_distance))

class YAxis_GPSElevation:
    axis_name = 'Ele [m]'
    def __init__(self, points, point_stats, val_min=None, val_max=None):
        if val_min is None:
            self.val_min = point_stats.ele_gps_min
        else:
            self.val_min = val_min
        if val_max is None:
            self.val_max = point_stats.ele_gps_max
        else:
            self.val_max = val_max

    def get_y(self, point_num, point):
        return point.elevation

    def stringify(self, val):
        return "{}".format(int(round(val)))

    def get_suggested_subticks(self, max_ticks):
        tick_distance = (self.val_max - self.val_min) / max_ticks
        if tick_distance == 0:
            return []
        tick_distance = util.round_up_reasonably(tick_distance)
        tick_min = util.round_up_to_multiple(self.val_min, tick_distance)
        tick_min += tick_distance
        return list(range(tick_min, int(self.val_max), tick_distance))

class YAxis_DHMElevation:
    axis_name = 'Ele [m]'
    def __init__(self, points, point_stats, val_min=None, val_max=None):
        if val_min is None:
            self.val_min = point_stats.ele_gps_min
        else:
            self.val_min = val_min
        if val_max is None:
            self.val_max = point_stats.ele_gps_max
        else:
            self.val_max = val_max

    def get_y(self, point_num, point):
        return point.dhm_elevation

    def stringify(self, val):
        return "{}".format(int(round(val)))

    def get_suggested_subticks(self, max_ticks):
        tick_distance = (self.val_max - self.val_min) / max_ticks
        if tick_distance == 0:
            return []
        tick_distance = util.round_up_reasonably(tick_distance)
        tick_min = util.round_up_to_multiple(self.val_min, tick_distance)
        tick_min += tick_distance
        return list(range(tick_min, int(self.val_max), tick_distance))

class GPSTrackGrapher:
    def __init__(self, points):
        self.points = points

        self.bg_color = wx.Colour(250, 250, 250)
        self.gridline_color = wx.Colour(230, 230, 230)
        self.border_px = 10
        self.tick_px = 3

        self.draw_gps_ele = True
        self.draw_dhm_ele = True
        self.x_axis_class = XAxis_Time

    @staticmethod
    def _value_to_pixel(value, axis, pixel_width_height):
        return round(pixel_width_height *
                     (value - axis.val_min) / (axis.val_max - axis.val_min))

    @staticmethod
    def _round_boundaries(min_val, max_val):
        """Calculate display bounderies from min/max values

        Calculate a display window where the min/max values fit nicely.
        The window will be round-numbered:

        >>> _round_boundaries(1032, 1538)
            (1000, 1600)
        """

        delta = max_val - min_val
        if delta != 0:
            magnitude = int(math.log10(delta))
        else:
            magnitude = 0
        round_adj = 10**magnitude / 2
        rounded_min = round(min_val - round_adj, -magnitude)
        rounded_max = round(max_val + round_adj, -magnitude)
        return rounded_min, rounded_max

    def calc_value_boundaries(self, point_stats):
        """Find appropriate x/y min/max boundaries for graphing"""

        x_axis = self.x_axis_class(self.points, point_stats)
        if self.draw_gps_ele and self.draw_dhm_ele:
            gps_min, gps_max = self._round_boundaries(point_stats.ele_gps_min,
                                                      point_stats.ele_gps_max)
            dhm_min, dhm_max = self._round_boundaries(point_stats.ele_dhm_min,
                                                      point_stats.ele_dhm_max)
            y_val_min = min(gps_min, dhm_min)
            y_val_max = max(gps_max, dhm_max)
            y_axes = [YAxis_GPSElevation(self.points, point_stats,
                                         y_val_min, y_val_max),
                      YAxis_DHMElevation(self.points, point_stats,
                                         y_val_min, y_val_max)]
        elif self.draw_gps_ele:
            y_val_min, y_val_max = self._round_boundaries(
                   point_stats.ele_gps_min, point_stats.ele_gps_max)
            y_axes = [YAxis_GPSElevation(self.points, point_stats,
                                         y_val_min, y_val_max)]
        elif self.draw_dhm_ele:
            y_val_min, y_val_max = self._round_boundaries(
                   point_stats.ele_dhm_min, point_stats.ele_dhm_max)
            y_axes = [YAxis_DHMElevation(self.points, point_stats,
                                         y_val_min, y_val_max)]
        else:
            y_axes = []

        return x_axis, y_axes

    def draw_one_function(self, dc, coosys_tl, coosys_br,
                          x_axis, y_axis):
        chart_size = coosys_br - coosys_tl
        line_points = []
        for idx, point in enumerate(self.points):
            x_val = x_axis.get_x(idx, point)
            y_val = y_axis.get_y(idx, point)
            x = self._value_to_pixel(x_val, x_axis, chart_size.x)
            y = self._value_to_pixel(y_val, y_axis, chart_size.y)
            line_points.append(wx.Point(coosys_tl.x + x, coosys_br.y - y))
        dc.DrawLines(line_points, 0, 0)

    def draw_all_functions(self, dc, coosys_tl, coosys_br, point_stats,
                           x_axis, y_axes):
        colors = iter([wx.BLUE, wx.RED, wx.Colour(79, 47, 79)])
        for y_axis in y_axes:
            dc.Pen = wx.Pen(next(colors), 1)
            self.draw_one_function(dc, coosys_tl, coosys_br, x_axis, y_axis)

    def vertical_tick(self, dc, text, pos, tickmark=True):
        dc.Pen = wx.BLACK_PEN
        if tickmark:
            dc.DrawLine(pos.x, pos.y, pos.x - self.tick_px, pos.y)

        util.draw_text(dc, text, wx.Point(pos.x - self.tick_px, pos.y),
                       alignment=wx.ALIGN_RIGHT|wx.ALIGN_CENTER_VERTICAL)

    def horizontal_tick(self, dc, text, pos, tickmark=True):
        dc.Pen = wx.BLACK_PEN
        if tickmark:
            dc.DrawLine(pos.x, pos.y, pos.x, pos.y + self.tick_px)

        util.draw_text(dc, text, wx.Point(pos.x, pos.y + self.tick_px),
                       alignment=wx.ALIGN_CENTER_HORIZONTAL|wx.ALIGN_TOP)

    def vertical_label(self, dc, text, pos):
        base_point = wx.Point(pos.x - self.tick_px, self.border_px)
        util.draw_text(dc, text, base_point,
                       alignment=wx.ALIGN_RIGHT|wx.ALIGN_TOP)

    def horizontal_label(self, dc, text, pos):
        base_point = wx.Point(dc.Size.width - 1 - self.border_px,
                              pos.y + self.tick_px)
        util.draw_text(dc, text, base_point,
                       alignment=wx.ALIGN_RIGHT|wx.ALIGN_TOP)

    def draw_coordinate_system(self, dc, coosys_tl, coosys_br):
        dc.Pen = wx.TRANSPARENT_PEN
        dc.Brush = wx.WHITE_BRUSH
        dc.DrawRectangle(coosys_tl, coosys_br - coosys_tl)

        coosys_bl = wx.Point(coosys_tl.x, coosys_br.y)
        dc.Pen = wx.BLACK_PEN
        dc.DrawLine(coosys_tl, coosys_bl)
        dc.DrawLine(coosys_bl, coosys_br)

    def gridline_x(self, dc, x_coord, coosys_tl, coosys_br):
        dc.Pen = wx.Pen(self.gridline_color, 1)
        dc.DrawLine(x_coord, coosys_tl.y, x_coord, coosys_br.y)

    def gridline_y(self, dc, y_coord, coosys_tl, coosys_br):
        # Use coosys_tl.x + 1 to avoid painting over the coordinate axis.
        dc.Pen = wx.Pen(self.gridline_color, 1)
        dc.DrawLine(coosys_tl.x + 1, y_coord, coosys_br.x, y_coord)

    def draw_tickmarks_gridlines(self, dc, coosys_tl, coosys_bl, coosys_br,
                                 x_est_tick_width, y_est_tick_height,
                                 x_axis, y_axis):

        chart_size = coosys_br - coosys_tl

        # Draw the outermost tickmarks first (i.e. begin/end of axis).
        self.vertical_tick(dc, y_axis.stringify(y_axis.val_max), coosys_tl)
        self.vertical_tick(dc, y_axis.stringify(y_axis.val_min), coosys_bl)
        self.horizontal_tick(dc, x_axis.stringify(x_axis.val_min), coosys_bl)
        self.horizontal_tick(dc, x_axis.stringify(x_axis.val_max), coosys_br)

        # [xy]_est_tick_[wh] estimates the size of a tick label and we can
        # infer how many ticks we can fit. The factors (1.*) ensure that we
        # have some room for larger labels and also bring sensible spacing.
        #
        # Y is tighter as heights vary less, and vertically close text doesn't
        # look as bad as horizontally close text.
        x_tick_spacing = int(x_est_tick_width * 1.7)
        x_tick_space_avail = (coosys_br.x - coosys_bl.x - x_est_tick_width)
        x_num_ticks = x_tick_space_avail / x_tick_spacing
        for x_tick in x_axis.get_suggested_subticks(x_num_ticks):
            x_tick_px = self._value_to_pixel(x_tick, x_axis, chart_size.x)
            x_tick_px += coosys_bl.x
            self.gridline_x(dc, x_tick_px, coosys_tl, coosys_br)
            # Avoid painting over the (manually drawn) last tick label.
            if x_tick_px < coosys_br.x - x_est_tick_width:
                self.horizontal_tick(dc, x_axis.stringify(x_tick),
                                     wx.Point(x_tick_px, coosys_bl.y))

        y_tick_spacing = int(y_est_tick_height * 1.3)
        y_tick_space_avail = (coosys_bl.y - coosys_tl.y - y_tick_spacing)
        y_num_ticks = y_tick_space_avail / y_tick_spacing
        for tick in y_axis.get_suggested_subticks(y_num_ticks):
            tick_px = self._value_to_pixel(tick, y_axis, chart_size.y)
            tick_px = coosys_bl.y - tick_px
            self.gridline_y(dc, tick_px, coosys_tl, coosys_br)
            # Avoid painting over the (manually drawn) last tick label.
            if tick_px > coosys_tl.y + y_est_tick_height:
                self.vertical_tick(dc, y_axis.stringify(tick),
                                   wx.Point(coosys_tl.x, tick_px))

    def draw_dc(self, dc):
        dc.BackgroundMode = wx.TRANSPARENT

        dc.Background = wx.Brush(self.bg_color)
        dc.Clear()

        if not dc.CanGetTextExtent():
            util.Error(self, _("Failed to draw GPS track:\n" +
                               "Can not calculate text size on this platform"))
            return
        label_width, label_height, label_descent, label_external_lead = \
                                             dc.GetFullTextExtent("+0000")

        point_stats = GPSPointStatistics(self.points)
        x_axis, y_axes = self.calc_value_boundaries(point_stats)

        if not y_axes:
            # No functions to draw: stop drawing alltogether.
            return

        y_axis = y_axes[0]
        x_last_tick_text = x_axis.stringify(x_axis.val_max)
        y_last_tick_text = y_axis.stringify(y_axis.val_max)

        x_label_width = dc.GetFullTextExtent(x_axis.axis_name)[0]
        y_label_height = dc.GetFullTextExtent(y_axis.axis_name)[1]
        x_last_tick_width = dc.GetFullTextExtent(x_last_tick_text)[0]
        y_last_tick_height = dc.GetFullTextExtent(y_last_tick_text)[1]

        coosys_tl = wx.Point(self.border_px + label_width + self.tick_px,
                             self.border_px + y_label_height +
                                              y_last_tick_height // 2)
        coosys_br = wx.Point(
                        dc.Size.width - 1 -
                            self.border_px - x_label_width -
                            self.border_px - x_last_tick_width // 2,
                        dc.Size.height - 1 -
                            self.border_px - label_height - self.tick_px)
        coosys_bl = wx.Point(coosys_tl.x, coosys_br.y)

        self.draw_coordinate_system(dc, coosys_tl, coosys_br)

        self.horizontal_label(dc, x_axis.axis_name, coosys_br)
        self.vertical_label(dc, y_axis.axis_name, coosys_tl)

        self.draw_tickmarks_gridlines(
                dc, coosys_tl, coosys_bl, coosys_br,
                x_last_tick_width, y_last_tick_height, x_axis, y_axis)

        if len(self.points) <= 1:
            # Don't draw any functions if we only have 0 or 1 points.
            return

        # Note that we pass all y_axes, not just y_axis == y_axes[0].
        self.draw_all_functions(dc, coosys_tl, coosys_br,
                                point_stats,
                                x_axis, y_axes)

    def draw(self, bmp):
        with util.Bitmap_MemoryDC_Context(bmp) as dc:
            self.draw_dc(dc)


class GPSTrackAnalyzerFrame(wx.Frame):
    def __init__(self, parent, track, heightfinder):
        wx.Frame.__init__(self)
        # 3-argument LoadFrame() calls self.Create(), so skip 2-phase creation.
        res = util.get_resources("main")
        if not res.LoadFrame(self, parent, "GPSTrackAnalyzerFrame"):
            raise RuntimeError("Could not load track analyzer frame from XRC.")

        util.bind_decorator_events(self)

        self.parent = parent
        self.track = track
        self.heightfinder = heightfinder
        self.pointlistbox = xrc.XRCCTRL(self, 'PointList')
        self.hdist_per_hour_text = xrc.XRCCTRL(self, 'HDistPerHourText')
        self.ele_per_hour_text = xrc.XRCCTRL(self, 'ElePerHourText')
        self.graphbitmap = xrc.XRCCTRL(self, 'GraphBitmap')

        self.stat_texts = [
                xrc.XRCCTRL(self, 'TotalTimeText'),
                xrc.XRCCTRL(self, 'HorizDistanceText'),
                xrc.XRCCTRL(self, 'VertDistanceGPSText'),
                xrc.XRCCTRL(self, 'VertDistanceDHMText'),
                ]
        self.stat_texts = {st: st.LabelText for st in self.stat_texts}

        fname = self.track.GetFname()
        self.gpx_basename = os.path.basename(fname).split(':', 1)[0]
        self.gpx_fname = os.path.join(os.path.dirname(fname),
                                      self.gpx_basename)

        self.gpx = self.track.data.gpx
        if not self.gpx:
            # We take the GPX data from an already loaded track.
            # This really shouldn't happen.
            raise RuntimeError("GPS Analyzer couldn't get gpx from track.")
        self.augment_gpx_data()

        self.active_segments = []
        for track in self.gpx.tracks:
            for segment in track.segments:
                self.active_segments.append(segment)

        self.populate_pointlistbox()

        self.grapher = GPSTrackGrapher(list(self.walk_active_points()))
        self.update_display()

    @util.EVENT(wx.EVT_SIZE, id=xrc.XRCID('GraphBitmap'))
    def on_graph_resize(self, evt):
        evt.Skip()
        self.update_display()

    @util.EVENT(wx.EVT_LIST_ITEM_SELECTED, id=xrc.XRCID('PointList'))
    @util.EVENT(wx.EVT_LIST_ITEM_DESELECTED, id=xrc.XRCID('PointList'))
    def on_pointlist_selection_change(self, evt):
        self.update_display()

    @util.EVENT(wx.EVT_PAINT, id=xrc.XRCID('GraphBitmap'))
    def on_graph_paint(self, evt):
        evt.Skip()
        self.grapher.points = list(self.walk_active_points())
        dc = wx.PaintDC(self.graphbitmap)
        self.grapher.draw_dc(dc)

    @util.EVENT(wx.EVT_PAINT, id=xrc.XRCID('TotalTimeText'))
    def on_output_paint(self, evt):
        if not self.output_needs_update:
            return
        self.output_needs_update = False

        points = list(self.walk_active_points())
        stats = GPSPointStatistics(points)
        stats.time_elapsed = {
                "h": stats.time_secs_elapsed // 3600,
                "m": (stats.time_secs_elapsed // 60) % 60,
                "s": stats.time_secs_elapsed % 60,
                }
        for st, format_string in self.stat_texts.items():
            st.LabelText = format_string.format(stats=stats)
        self.stats = stats

    @util.EVENT(wx.EVT_TEXT_ENTER, id=xrc.XRCID('HDistPerHourText'))
    def on_hdist_per_hour_enter(self, evt):
        km_per_hour = float(self.hdist_per_hour_text.Value)
        h_total_km = self.stats.total_distance / 1000
        h_time_hours = h_total_km / km_per_hour

        v_time_hours = self.waytime_calc_inverse(h_time_hours)
        v_Hm_per_hour = self.stats.ele_dhm_delta / v_time_hours
        self.ele_per_hour_text.Value = "{:.0f}".format(v_Hm_per_hour)

    @util.EVENT(wx.EVT_TEXT_ENTER, id=xrc.XRCID('ElePerHourText'))
    def on_ele_per_hour_enter(self, evt):
        Hm_per_hour = float(self.ele_per_hour_text.Value)
        v_time_hours = self.stats.ele_dhm_delta / Hm_per_hour

        h_time_hours = self.waytime_calc_inverse(v_time_hours)
        h_total_km = self.stats.total_distance / 1000
        h_km_per_hour = h_total_km / h_time_hours
        self.hdist_per_hour_text.Value = "{:.0f}".format(h_km_per_hour)

    def waytime_calc(self, one_dimension_hours, other_dimension_hours):
        return max(one_dimension_hours, other_dimension_hours) + \
               min(one_dimension_hours, other_dimension_hours) / 2

    def waytime_calc_inverse(self, one_dimension_hours):
        total_hours = self.stats.time_secs_elapsed / 3600
        if one_dimension_hours >= total_hours * 2 / 3:
            # Contribution of this dimension is larger.
            return 2 * (total_hours - one_dimension_hours)
        else:
            # Contribution of the other dimension is larger.
            return total_hours - (one_dimension_hours / 2)

    def augment_gpx_data(self):
        first_point = self.gpx.tracks[0].segments[0].points[0]
        for point, track_idx, seg_idx, point_idx in self.gpx.walk():
            point.abs_name = "%s:%d:%d:%d" % (self.gpx_basename, track_idx,
                                              seg_idx, point_idx)

            latlon = pymaplib.LatLon(point.latitude, point.longitude)
            ok, terraininfo = self.heightfinder.CalcTerrain(latlon)
            if not ok:
                terraininfo.height_m = -1
            point.dhm_elevation = terraininfo.height_m

            point.secs_after_start = point.time_difference(first_point)

    def populate_pointlistbox(self):
        self.pointlistbox.AppendColumn(_("Time"))
        self.pointlistbox.AppendColumn(_("Name"))
        self.pointlistbox.AppendColumn(_("Lat"))
        self.pointlistbox.AppendColumn(_("Lon"))
        self.pointlistbox.AppendColumn(_("GPS Ele"))
        self.pointlistbox.AppendColumn(_("Map Ele"))
        self.pointlistbox.AppendColumn(_("Delta Ele"))

        self.point_list_map = dict()
        for point in self.walk_all_points():
            tm_struct = time.gmtime(point.secs_after_start)
            self.pointlistbox.Append([
                      time.strftime("%H:%M:%S", tm_struct),
                      point.abs_name,
                      "{:1.06f}".format(point.latitude),
                      "{:1.06f}".format(point.longitude),
                      "{:1.01f}".format(point.elevation),
                      "{:1.01f}".format(point.dhm_elevation),
                      "{:1.01f}".format(point.dhm_elevation - point.elevation),
                      ])
            self.point_list_map[point] = self.pointlistbox.GetItemCount() - 1

    def update_display(self):
        self.graphbitmap.Refresh()
        self.output_needs_update = True
        xrc.XRCCTRL(self, 'TotalTimeText').Refresh()

    def walk_all_points(self):
        for seg in self.active_segments:
            for point in seg.walk(only_points=True):
                yield point

    def walk_active_points(self):
        if self.pointlistbox.GetSelectedItemCount() == 0:
            yield from self.walk_all_points()

        for seg in self.active_segments:
            for point in seg.walk(only_points=True):
                list_index = self.point_list_map[point]
                if self.pointlistbox.IsSelected(list_index):
                    yield point


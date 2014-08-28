import math

import gpxpy
import gpxpy.gpx

from . import maplib_sip as maplib_sip
from .errors import MapLibError, FileLoadError, FileOpenError, FileParseError

class GPSData:
    """GPS data backend"""

    def __init__(self, fname):
        """Load GPS data from a file or pass a GPX object directly"""

        self._fname = fname
        if isinstance(fname, str):
            try:
                with open(fname, 'r') as f:
                    self.gpx = gpxpy.parse(f)
            except FileNotFoundError as e:
                raise FileOpenError("Could not find file '%s':\n%s",
                                     fname, str(e)) from None
            except gpxpy.gpx.GPXException as e:
                raise FileParseError("Invalid GPX file '%s':\n%s",
                                     fname, str(e)) from None
        else:
            self.gpx = fname

        self.update_points()

    def update_points(self):
        self.all_points = []
        self.all_segments = []
        for track_idx, track in enumerate(self.gpx.tracks):
            self.all_segments.extend(track.segments)
            for segment_idx, segment in enumerate(track.segments):
                self.all_points.extend(segment.points)


class GPSTrack(maplib_sip.GeoDrawable):
    # Estimate m/degree to set a sane rendering resolution for GetRegion().
    EARTH_RADIUS_METERS = 6371000
    METERS_PER_DEGREE = EARTH_RADIUS_METERS * math.pi / 180
    METERS_PER_PIXEL = 10

    def __init__(self, fname, data=None):
        super().__init__()
        self._fname = fname
        self.data = data
        if self.data is None:
            self.data = GPSData(fname)

        if self.data.all_points:
            self._lat_min = min(p.latitude for p in self.data.all_points)
            self._lat_max = max(p.latitude for p in self.data.all_points)
            self._lon_min = min(p.longitude for p in self.data.all_points)
            self._lon_max = max(p.longitude for p in self.data.all_points)
        else:
            self._lat_min = 0
            self._lat_max = 90
            self._lon_min = 0
            self._lon_max = 90

        height = self._lat_max - self._lat_min
        width = self._lon_max - self._lon_min

        self._lat_min -= 0.05 * height
        self._lat_max += 0.05 * height
        self._lon_min -= 0.05 * width
        self._lon_max += 0.05 * width

        lat_delta_pixels = 1.1 * height * self.METERS_PER_DEGREE / self.METERS_PER_PIXEL
        lon_delta_pixels = 1.1 * width * self.METERS_PER_DEGREE / self.METERS_PER_PIXEL

        self._size = maplib_sip.MapPixelDeltaInt(lon_delta_pixels,
                                                 lat_delta_pixels)

        self.bg_color = 0xFF000000
        self.color = 0xFF0000FF
        self.hl_color = 0xFF00CC00

    def GetType(self):
        return maplib_sip.GeoDrawable.TYPE_GPSTRACK
    def GetWidth(self):
        return self._size.x
    def GetHeight(self):
        return self._size.y
    def GetSize(self):
        return self._size
    def GetFname(self):
        return self._fname
    def GetTitle(self):
        return self.data.gpx.name or ""
    def GetDescription(self):
        return self.data.gpx.description or ""
    def GetRegion(self, pos, size):
        # Not implemented for now, we want direct drawing anyway.
        return maplib_sip.PixelBuf()
    def GetProj(self):
        return maplib_sip.Projection("")

    def PixelToLatLon(self, pos):
        delta_lat = self._lat_max - self._lat_min
        delta_lon = self._lon_max - self._lon_min
        return True, maplib_sip.LatLon(
            pos.y / self._size.y * delta_lat + self._lat_min,
            pos.x / self._size.x * delta_lon + self._lon_min)

    def LatLonToPixel(self, pos):
        delta_lat = self._lat_max - self._lat_min
        delta_lon = self._lon_max - self._lon_min
        return True, maplib_sip.MapPixelCoord(
            (pos.lon - m_lon_min) / delta_lon * m_size.x,
            (pos.lat - m_lat_min) / delta_lat * m_size.y)

    def SupportsDirectDrawing(self):
        return True
    def GetPixelFormat(self):
        return maplib_sip.ODM_PIX_RGBA4
    def GetRegionDirect(self, output_size, base, base_tl, base_br):
        base_bl = maplib_sip.MapPixelCoord(base_tl.x, base_br.y)
        base_tr = maplib_sip.MapPixelCoord(base_br.x, base_tl.y)

        success_tl, latlon_tl = base.PixelToLatLon(base_tl)
        success_bl, lablon_bl = base.PixelToLatLon(base_bl)
        success_br, labron_br = base.PixelToLatLon(base_br)
        success_tr, latron_tr = base.PixelToLatLon(base_tr)

        if not (success_tl and success_bl and success_br and success_tr):
            return maplib_sip.PixelBuf()

        ll_list = [latlon_tl, lablon_bl, labron_br, latron_tr]
        lat_min = min(ll.lat for ll in ll_list)
        lat_max = max(ll.lat for ll in ll_list)
        lon_min = min(ll.lon for ll in ll_list)
        lon_max = max(ll.lon for ll in ll_list)

        out_of_bounds = (self._lat_min > lat_max or self._lat_max < lat_min or
                         self._lon_min > lon_max or self._lon_max < lon_min)
        if out_of_bounds:
            return maplib_sip.PixelBuf(output_size.x, output_size.y)

        result = maplib_sip.PixelBuf(output_size.x, output_size.y)
        base_scale_factor = output_size.x / (base_br.x - base_tl.x)

        def coord_iter(points):
            prev_coord = None
            for point in points:
                point_ll = maplib_sip.LatLon(point.latitude, point.longitude)
                success, point_abs = base.LatLonToPixel(point_ll)
                if not success:
                    raise RuntimeError("Could not draw GPS track on a "
                                       "non-georeferenced basemap.")
                curr_coord = maplib_sip.PixelBufCoord(
                    int(round((point_abs.x - base_tl.x) * base_scale_factor)),
                    int(round((point_abs.y - base_tl.y) * base_scale_factor)))
                yield curr_coord, prev_coord, point
                prev_coord = curr_coord

        for segment in self.data.all_segments:
            for curr_coord, prev_coord, point in coord_iter(segment.points):
                result.Rect(curr_coord - maplib_sip.PixelBufDelta(2, 2),
                            curr_coord + maplib_sip.PixelBufDelta(3, 3),
                            self.bg_color)
                if prev_coord is not None:
                    result.Line(prev_coord, curr_coord, 3, self.bg_color)

        for segment in self.data.all_segments:
            prev_highlight = False
            for curr_coord, prev_coord, point in coord_iter(segment.points):
                curr_highlight = getattr(point, 'highlight', False)
                curr_pt_color = self.hl_color if curr_highlight else self.color
                prev_pt_color = self.hl_color if prev_highlight else self.color
                line_color = self.color
                if curr_highlight and prev_highlight:
                    line_color = self.hl_color
                prev_highlight = curr_highlight

                if prev_coord is not None:
                    result.Line(prev_coord, curr_coord, line_color)
                    result.Rect(prev_coord - maplib_sip.PixelBufDelta(1, 1),
                                prev_coord + maplib_sip.PixelBufDelta(2, 2),
                                prev_pt_color)
                result.Rect(curr_coord - maplib_sip.PixelBufDelta(1, 1),
                            curr_coord + maplib_sip.PixelBufDelta(2, 2),
                            curr_pt_color)

        return result


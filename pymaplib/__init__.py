"""Pymaplib - A C++/Python library for reading and drawing maps"""

import os
import re
import contextlib

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *
from .errors import *

from . import filelist
from .filelist import FileList, FileListEntry
from .gpstracks import *

def _(s): return s


def parse_coordinate(s):
    """Parse Lat/Lon strings to LatLon objects

    Parse coordinates in a variety of formats to LatLon objects.
    Currently supported are DD.DDDDDD, DD MM.MMMM, DD MM SS.SS and UTM/UPS.
    """

    # Parse fractional degrees.
    # 47.605092,15.126536
    # N 47.71947°, O 15.68881 °
    m = re.match(r"""
            ^\s*
            [NS]?\s*
            (\d+.\d+)\s*
            °?\s*
            [,;]?\s*
            [EOW]?\s*
            (\d+.\d+)\s*
            °?\s*
            $
            """, s, re.VERBOSE)
    if m:
        return LatLon(float(m.group(1)), float(m.group(2)))

    # Parse integer degrees, fractional minutes.
    # N 47° 43.168', O 15 ° 41.329'
    m = re.match(r"""
                ^\s*
                [NS]?\s*
                (\d+)\s*°\s*
                (\d+.\d+)\s*'\s*
                [,;]?\s*
                [EOW]?\s*
                (\d+)\s*°\s*
                (\d+.\d+)\s*'\s*
                $
                """, s, re.VERBOSE)
    if m:
        g = m.groups()
        return LatLon(int(g[0]) + float(g[1])/60,
                      int(g[3]) + float(g[4])/60)

    # Parse integer degrees and minutes, possibl fractional seconds.
    # N 47° 43' 10.1", O 15 ° 41' 19.7"
    m = re.match(r"""
                ^\s*
                [NS]?\s*
                (\d+)\s*°\s*
                (\d+)\s*'\s*
                (\d+.?\d*)\s*"\s*
                [,;]?\s*
                [EOW]?\s*
                (\d+)\s*°\s*
                (\d+)\s*'\s*
                (\d+.?\d*)\s*"\s*
                $
                """, s, re.VERBOSE)
    if m:
        g = m.groups()
        return LatLon(int(g[0]) + int(g[1])/60 + float(g[2])/3600,
                      int(g[3]) + int(g[4])/60 + float(g[5])/3600)

    # Parse UTM, use north/south semantics. Zone letters are not recognized.
    # 33N 23456 4567890
    m = re.match(r"""
            ^\s*
            (\d*)\s*      # Zone, may be empty for UPS
            ([NS])\s+     # North/South
            (\d+)\s*      # Easting
            m?\s*
            [,;]?\s*
            (\d+)\s*      # Northing
            m?\s*
            $
            """, s, re.VERBOSE)
    if m:
        if m.group(1):
            # Numerical zone given: UTM.
            zone = int(m.group(1))
        else:
            # Numerical zone not given: UPS.
            zone = 0

        northp = (m.group(2) == 'N')
        x = float(m.group(3))
        y = float(m.group(4))
        return LatLon(UTMUPS(zone, northp, x, y))

    # Maybe handle more (non-standard) UTM coordinates here:
    # N 5285350 m, 33  551660 m
    return None

def format_coordinate(coord_fmt, latlon):
    """Format a LatLon coordinate as string

    coord_fmt determines the format; valid values are:
      "DDD": fractional degrees
      "DMM": full degrees, fractional minutes
      "DMS": full degrees and minutes, fractional seconds
      "UTM": universal transversal mercator format
    """

    if coord_fmt == "DDD":
        return _("{:0.06f}, {:0.06f}").format(latlon.lat, latlon.lon)
    elif coord_fmt == "DMM":
        m_lat = (abs(latlon.lat) % 1) * 60
        m_lon = (abs(latlon.lon) % 1) * 60
        return _("{:d}° {:07.04f}', {:d}° {:07.04f}'").format(
                                     int(latlon.lat), m_lat,
                                     int(latlon.lon), m_lon)
    elif coord_fmt == "DMS":
        m_lat = (abs(latlon.lat) % 1) * 60
        m_lon = (abs(latlon.lon) % 1) * 60
        fmt = _('''{:d}° {:02d}' {:05.02f}", {:d}° {:02d}' {:05.02f}"''')
        return fmt.format(int(latlon.lat), int(m_lat), (m_lat % 1) * 60,
                          int(latlon.lon), int(m_lon), (m_lon % 1) * 60)
    elif coord_fmt == "UTM":
        utm = UTMUPS(latlon)
        # Omit zone number for UPS coordinates.
        zone = _("{:02d}").format(utm.zone) if utm.zone != 0 else ""
        return _("{}{} {:.0f} {:.0f}").format(zone,
                                              "N" if utm.northp else "S",
                                              utm.x, utm.y)
    else:
        raise NotImplementedError("Unknown coordinate format: '%s'",
                                  coord_fmt)

class HeightFinder:
    """Provider DHM based services

    Offer two services related to DHMs: finding the best available DHM for a
    particular spot, and calculating height/slope steepness/slope orientation.
    """

    # Starting with SRTM, most DHMs use -32768 as invalid pixel value (i.e. no
    # height information available).
    # Cf. https://www.google.com/search?q=32768+srtm
    INVALID_DHM_VALUE = -2**15

    def __init__(self, maps):
        """HeightFinder constructor

        Argument ``maps`` can the full maplist, not only the DHMs to work on.
        """

        super().__init__()
        self.maps = maps

    def _mpp_or_inf(self, dhm, latlon):
        """Maps/Pixel of a given DHM, or float("inf") on error"""

        ok, coord = dhm.LatLonToPixel(latlon)
        if not ok:
            return float("inf")
        ok, mpp = MetersPerPixel(dhm, coord)
        if not ok:
            return float("inf")
        return mpp

    def find_best_dhms(self, latlon):
        """Return a list of available DHMs for a given location

        The list is sorted by decreasing resolution, i.e. the first element
        is the most preferable.
        """

        type_dhm = maplib_sip.GeoDrawable.TYPE_DHM
        dhms = [container for container in self.maps
                if container.drawable.GetType() == type_dhm]
        local_dhms = [container for container in dhms
                      if is_within_map(latlon, container.drawable)]
        return sorted(local_dhms,
                      key=lambda cont: self._mpp_or_inf(cont.drawable, latlon))

    def calc_terrain(self, latlon):
        """Calculate terrain info for a specific location

        Returns a tuple ``(ok, terrain_info)``, where ``terrain_info``
        is only valid if ``ok`` is True.

        ``terrain_info`` has 3 members:
            height_m, slope_face_deg, steepness_deg
        """

        dhms = self.find_best_dhms(latlon)
        for dhm in dhms:
            ok, res = maplib_sip.CalcTerrainInfo(dhm.drawable, latlon)
            if ok and res.height_m != self.INVALID_DHM_VALUE:
                return ok, res
        return False, None


def is_within_map(latlon, drawable):
    """Return True if the point latlon lies within drawable"""

    ok, coord = drawable.LatLonToPixel(latlon)
    if not ok:
        return False
    return coord.IsInRect(MapPixelCoordInt(0, 0),
                          drawable.GetSize())

def larger_scale_maps(current_drawable, latlon, maplist):
    """Find larger-scale maps of the same type as current_drawable

    Return a list of drawables, sorted by ascending meters-per-pixel values.
    All of the returned maps have a larger scale than current_drawable (i.e.
    lower mpp values).
    """

    return _other_scale_maps(current_drawable, latlon, maplist,
                             lambda new_mpp, cur_mpp: new_mpp < cur_mpp)

def smaller_scale_maps(current_drawable, latlon, maplist):
    """Find smaller-scale maps of the same type as current_drawable

    Return a list of drawables, sorted by ascending meters-per-pixel values.
    All of the returned maps have a smaller scale than current_drawable (i.e.
    higher mpp values).
    """

    return _other_scale_maps(current_drawable, latlon, maplist,
                             lambda new_mpp, cur_mpp: new_mpp > cur_mpp)

def _other_scale_maps(current_drawable, latlon, maplist, mpp_selector):
    """Backend for larger/smaller_scale_maps"""

    ok, cur_mpp = MetersPerPixel(current_drawable, MapPixelCoordInt(0, 0))
    if not ok:
        return []

    candidates = []
    for container in maplist:
        if container.drawable:
            candidates.append(container.drawable)
        candidates.extend(container.alternate_views)

    viable_maps = []
    for drawable in candidates:
        if drawable == current_drawable:
            continue
        if drawable.GetType() != current_drawable.GetType():
            continue
        if not is_within_map(latlon, drawable):
            continue

        ok, mpp = MetersPerPixel(drawable, MapPixelCoordInt(0, 0))
        if not ok:
            continue
        if not mpp_selector(mpp, cur_mpp):
            continue
        viable_maps.append((mpp, drawable))
    return [drawable for mpp, drawable in sorted(viable_maps)]


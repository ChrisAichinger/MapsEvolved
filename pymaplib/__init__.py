import os
import re
import contextlib

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *
from .errors import *

from . import filelist
from .filelist import FileList, FileListEntry

def _(s): return s



class DefaultPersistentStore:
    """The default PersistenStore wrapped in a Python context manager"""

    @staticmethod
    @contextlib.contextmanager
    def Read(ps=None):
        if ps is None:
            ps = maplib_sip.CreatePersistentStore()
        ps.OpenRead()
        try:
            yield ps
        finally:
            ps.Close()

    @staticmethod
    @contextlib.contextmanager
    def Write(ps=None):
        if ps is None:
            ps = maplib_sip.CreatePersistentStore()
        ps = maplib_sip.CreatePersistentStore()
        ps.OpenWrite()
        try:
            yield ps
        finally:
            ps.Close()


def parse_coordinate(s):
    """Parse Lat/Lon strings to LatLon objects

    Parse coordinates in a variety of formats to LatLon objects.
    Currently supported are DD.DDDDDD, DD MM.MMMM, DD MM SS.SS.
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
        return float(m.group(1)), float(m.group(2))

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
        return (int(g[0]) + float(g[1])/60,
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
        return (int(g[0]) + int(g[1])/60 + float(g[2])/3600,
                int(g[3]) + int(g[4])/60 + float(g[5])/3600)

    # TODO: Handle UTM
    # N 5285350 m, 33  551660 m
    return None

def format_coordinate(coord_fmt, latlon):
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

class HeightFinder(maplib_sip.HeightFinder):
    def __init__(self, maps):
        super().__init__()
        self.maps = maps

    def FindBestMap(self, pos, map_type):
        for container in self.maps:
            if container.drawable.GetType() == map_type:
                return container.drawable
        return None

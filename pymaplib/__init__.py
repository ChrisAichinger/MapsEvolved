import os
import re
import contextlib
import collections

import gpxpy
import gpxpy.gpx

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *
from .errors import *

from . import poidb

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


class GeoFilesCollection:
    ItemViews = collections.namedtuple('ItemViews', ['item', 'views'])
    def __init__(self):
        self.maplist = []
        self.gpxlist = []
        self.dblist = []

    def add_file(self, fname, ftype=None):
        if ftype is None:
            lname = fname.lower()
            if lname.endswith('.gpx'):
                ftype = 'GPX'
            elif lname.endswith('.db'):
                ftype = 'DB'
            elif lname.endswith('.tif') or lname.endswith('.tiff'):
                ftype = 'TIF'

        if ftype == "GPX":
            self._add_gpx(fname)
        elif ftype == 'DB':
            self._add_db(fname)
        elif ftype == 'TIF':
            self._add_geotiff(fname)
        else:
            raise NotImplementedError("Couldn't detect file type")

    def _add_gpx(self, fname):
        try:
            with open(fname, 'r') as f:
                gpx = gpxpy.parse(f)
        except (gpxpy.gpx.GPXException, FileNotFoundError):
            raise  # TODO: Transform this into one of our own exceptions.

        segments = []
        all_points = []
        for track_idx, track in enumerate(gpx.tracks):
            for segment_idx, segment in enumerate(track.segments):
                points = [LatLon(p.latitude, p.longitude)
                          for p in segment.points]
                all_points.extend(points)
                segment_name = "%s:%d:%d" % (fname, track_idx, segment_idx)
                gpx_segment = GPSSegmentShPtr(segment_name, points)
                segments.append(gpx_segment)

        gpx_file = GPSSegmentShPtr(fname, all_points)
        listitem = GeoFilesCollection.ItemViews(gpx_file, segments)
        self.gpxlist.append(listitem)

    def _add_db(self, fname):
        db = poidb.POI_Database(fname)
        self.dblist.append(GeoFilesCollection.ItemViews(db, []))

    def _add_geotiff(self, fname):
        rastermap = LoadMap(fname)
        views = AlternateMapViews(rastermap)
        self.maplist.append(GeoFilesCollection.ItemViews(rastermap, views))

    def store_to(self, store):
        def do_store(ps):
            ps.SetStringList('maps',
                    [map_entry.item.GetFname() for map_entry in self.maplist])
            ps.SetStringList('gpxlist',
                    [gpx_entry.item.GetFname() for gpx_entry in self.gpxlist])
            ps.SetStringList('dblist',
                    [db_entry.item.fname for db_entry in self.dblist])

        if not store.IsOpen():
            with DefaultPersistentStore.Write(store) as ps:
                do_store(ps)
        else:
            do_store(store)

    def retrieve_from(self, store):
        def do_retrieve(ps, name, ftype):
            # Exceptions reading from the persistentstore are likely
            # from running the first time - ignore them.
            try:
                files = ps.GetStringList(name)
            except RuntimeError:
                pass
            else:
                for fname in files:
                    self.add_file(fname, ftype=ftype)

        if not store.IsOpen():
            with DefaultPersistentStore.Read(store) as ps:
                do_retrieve(ps, 'maps', 'TIF')
                do_retrieve(ps, 'gpxlist', 'GPX')
                do_retrieve(ps, 'dblist', 'DB')
        else:
            do_retrieve(store, 'maps', 'TIF')
            do_retrieve(store, 'gpxlist', 'GPX')
            do_retrieve(store, 'dblist', 'DB')

    def is_toplevel(self, item):
        return (any(True for e in self.maplist if e.item == item) or
                any(True for e in self.gpxlist if e.item == item) or
                any(True for e in self.dblist if e.item == item))

    def _delete(self, item, lst, errormsg):
        for i, entry in enumerate(lst):
            if entry.item == item:
                del lst[i]
                return
        raise RuntimeError(errormsg)

    def delete(self, item):
        if item.GetType() == GeoDrawable.TYPE_POI_DB:
            self._delete(item, self.dblist, "POI DB file to delete not found?")
        elif item.GetType() == GeoDrawable.TYPE_GPSTRACK:
            self._delete(item, self.gpxlist, "GPX file to delete not found?")
        else:
            self._delete(item, self.maplist, "Map file to delete not found?")


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
        for map, views in self.maps:
            if map.GetType() == map_type:
                self.active_map = map
                return self.active_map
        return None

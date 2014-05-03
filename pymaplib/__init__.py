import os
import re
import contextlib
import collections

import gpxpy
import gpxpy.gpx

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *

from . import poidb

def _(s): return s


class MapLibError(RuntimeError):
    pass

class FileLoadError(MapLibError):
    pass

class FileOpenError(FileLoadError):
    pass

class FileParseError(FileLoadError):
    pass


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
        if not os.access(fname, os.R_OK):
            raise FileOpenError("Could not access file: '%s'", fname)
        try:
            db = poidb.POI_Database(fname)
        except poidb.sqlite3.Error:
            raise FileParseError("Not a valid database: '%s'", fname)

        self.dblist.append(GeoFilesCollection.ItemViews(db, []))

    def _add_geotiff(self, fname):
        map = LoadMap(fname)
        reps = AlternateMapViews(map)
        self.maplist.append(GeoFilesCollection.ItemViews(map, reps))

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
        def do_retrieve(ps):
            # Exceptions reading from the persistentstore are likely
            # from running the first time - ignore them.
            try:
                mapfiles = ps.GetStringList('maps')
            except RuntimeError:
                pass
            else:
                for fname in mapfiles:
                    self.add_file(fname, ftype='TIF')

            try:
                gpxnames = ps.GetStringList('gpxlist')
            except RuntimeError:
                pass
            else:
                for fname in gpxnames:
                    self.add_file(fname, ftype='GPX')

            try:
                dbnames = ps.GetStringList('dblist')
            except RuntimeError:
                pass
            else:
                for fname in dbnames:
                    self.add_file(fname, ftype='DB')

        if not store.IsOpen():
            with DefaultPersistentStore.Read(store) as ps:
                do_retrieve(ps)
        else:
            do_retrieve(store)

    def is_toplevel(self, item):
        return (any(True for m, views in self.maplist if m == item) or
                any(True for g, views in self.gpxlist if g == item) or
                any(True for d, views in self.dblist if d == item))

    def delete_map(self, rastermap):
        for i, map_entry in enumerate(self.maplist):
            if map_entry.item == rastermap:
                del self.maplist[i]
                return
        raise RuntimeError("map to delete not found?")

    def delete_gpx(self, gpx):
        for i, gpx_entry in enumerate(self.gpxlist)):
            if gpx_entry.item == gpx:
                del self.gpxlist[i]
                return
        raise RuntimeError("gpx file to delete not found?")

    def delete_db(self, db):
        for i, db_entry in enumerate(self.dblist)):
            if db_entry.item == db
                del self.dblist[i]
                return
        raise RuntimeError("POI DB to delete not found?")

    def delete(self, item):
        if item.GetType() == GeoDrawable.TYPE_POI_DB:
            self.delete_db(item)
        elif item.GetType() == GeoDrawable.TYPE_GPSTRACK:
            self.delete_gpx(item)
        else:
            self.delete_map(item)


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
        maplib_sip.HeightFinder.__init__(self)
        self.maps = maps

    def FindBestMap(self, pos, map_type):
        for map, views in self.maps:
            if map.GetType() == map_type:
                self.active_map = map
                return self.active_map
        return None

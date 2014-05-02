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


#class PythonRasterMapCollection(DerivableRasterMapCollection):
#    MapsAndReps = collections.namedtuple('MapsAndReps', ['map', 'views'])
#    def __init__(self):
#        DerivableRasterMapCollection.__init__(self)
#        self.lst = []
#
#    def AddMap(self, rastermap):
#        views = []
#        if rastermap.GetType == GeoDrawable.TYPE_DHM:
#            views.append(RasterMapShPtr(GradientMap(rastermap)))
#            views.append(RasterMapShPtr(SteepnessMap(rastermap)))
#        self.lst.append(self.MapsAndReps(rastermap, views))
#
#    def DeleteMap(self, index):
#        del self.lst[index]
#
#    def Size(self):
#        return len(self.lst)
#
#    def Get(self, index):
#        return self.lst[index].map
#
#    def GetAlternateRepresentations(self, index):
#        return self.lst[i].views
#
#    def IsToplevelMap(self, rastermap):
#        return any(True for map, views in self.lst if map == rastermap)
#
#    def StoreTo(self, store):
#        if not store.IsOpen():
#            store.OpenWrite()
#        store.SetStringList("maps", [m.GetFname() for m, views in self.lst])
#
#    def LoadFrom(self, store):
#        if not store.IsOpen():
#            store.OpenRead()
#        for mapname in store.GetStringList("maps"):
#            LoadMap(self, mapname)

class GeoFilesCollection:
    ItemViews = collections.namedtuple('ItemViews', ['item', 'views'])
    def __init__(self, rastermapcollection):
        self._maplist = rastermapcollection
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
        LoadMap(self._maplist, fname)

    def store_to(self, store):
        def do_store(ps):
            self._maplist.StoreTo(ps)
            ps.SetStringList('gpxlist',
                             [gpxfile.GetFname()
                              for gpxfile, segments in self.gpxlist])
            ps.SetStringList('dblist', [db.fname for db, _ in self.dblist])

        if not store.IsOpen():
            with DefaultPersistentStore.Write(store) as ps:
                do_store(ps)
        else:
            do_store(store)

    def retrieve_from(self, store):
        def do_retrieve(ps):
            self._maplist.RetrieveFrom(ps)

            gpxnames = []
            try:
                gpxnames = ps.GetStringList('gpxlist')
            except RuntimeError:
                # Most likely no gpxlist has been created in the PS yet.
                # Silently ignore the error
                pass
            for fname in gpxnames:
                self.add_file(fname, ftype='GPX')

            dbnames = []
            try:
                dbnames = ps.GetStringList('dblist')
            except RuntimeError:
                # Most likely no dblist has been created in the PS yet.
                # Silently ignore the error
                pass
            for fname in dbnames:
                self.add_file(fname, ftype='DB')

        if not store.IsOpen():
            with DefaultPersistentStore.Read(store) as ps:
                do_retrieve(ps)
        else:
            do_retrieve(store)

    @property
    def maplist(self):
        result = []
        for i in range(self._maplist.Size()):
            result.append(GeoFilesCollection.ItemViews(
                    self._maplist.Get(i),
                    self._maplist.GetAlternateRepresentations(i)))
        return result

    def is_toplevel(self, item):
        return (any(True for m, views in self.maplist if m == item) or
                any(True for g, views in self.gpxlist if g == item) or
                any(True for d, views in self.dblist if d == item))

    def delete_map(self, rastermap):
        for i in range(self._maplist.Size()):
            if self._maplist.Get(i) == rastermap:
                break
        else:
            raise RuntimeError("map to delete not found?")

        self._maplist.DeleteMap(i)

    def delete_gpx(self, rastermap):
        for i in range(self._maplist.Size()):
            if self._maplist.Get(i) == rastermap:
                break
        else:
            raise RuntimeError("map to delete not found?")

        self._maplist.DeleteMap(i)
    def delete_db(self, db):
        for i in range(len(self.dblist)):
            if db == self.dblist[i].item:
                del self.dblist[i]
                break
        else:
            raise RuntimeError("POI DB to delete not found?")

    def delete(self, item):
        if item.GetType() == GeoDrawable.TYPE_POI_DB:
            self.delete_db(item)
        elif item.GetType() == GeoDrawable.TYPE_GPSTRACK:
            self.delete_gpx(item)
        else:
            self.delete_map(item)


def parse_coordinates(s):
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

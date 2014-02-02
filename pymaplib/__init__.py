import contextlib
import collections

import gpxpy
import gpxpy.gpx

# Load all the SIP wrappers into here
from . import maplib_sip as maplib_sip
from .maplib_sip import *


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

    def add_file(self, fname, ftype=None):
        if ftype is None:
            lname = fname.lower()
            if lname.endswith('.gpx'):
                ftype = 'GPX'
            elif lname.endswith('.tif') or lname.endswith('.tiff'):
                ftype = 'TIF'
        if ftype == "GPX":
            self._add_gpx(fname)
        elif ftype == 'TIF':
            self._add_geotiff(fname)
        else:
            raise NotImplementedError("Couldn't detect file type")

    def _add_gpx(self, fname):
        try:
            with open(fname, 'r') as f:
                gpx = gpxpy.parse(f)
        except gpxpy.gpx.GPXException:
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

    def _add_geotiff(self, fname):
        LoadMap(self._maplist, fname)

    def store_to(self, store):
        def do_store(ps):
            self._maplist.StoreTo(ps)
            fnames = []
            for gpxfile, segments in self.gpxlist:
                fnames.append(gpxfile.GetFname())
            ps.SetStringList('gpxlist', fnames)

        if not store.IsOpen():
            with DefaultPersistentStore.Write(store) as ps:
                do_store(ps)
        else:
            do_store(store)

    def retrieve_from(self, store):
        def do_retrieve(ps):
            self._maplist.RetrieveFrom(ps)
            try:
                fnames = ps.GetStringList('gpxlist')
            except RuntimeError:
                return
            for fname in fnames:
                self.add_file(fname, ftype='GPX')

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
                any(True for g, views in self.gpxlist if g == item))

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

    def delete(self, item):
        if item.GetType() == GeoDrawable.TYPE_GPSTRACK:
            self.delete_gpx(item)
        else:
            self.delete_map(item)


import os
import sqlite3
import unicodedata

import gpxpy
import gpxpy.gpx

from . import maplib_sip as maplib_sip
from .errors import MapLibError, FileLoadError, FileOpenError, FileParseError
from .gpstracks import GPSTrack

def _(s): return s


class FileListEntry:
    TYPE_MAP = maplib_sip.GeoDrawable.TYPE_MAP
    TYPE_DHM = maplib_sip.GeoDrawable.TYPE_DHM
    TYPE_GRADIENT_MAP = maplib_sip.GeoDrawable.TYPE_GRADIENT_MAP
    TYPE_STEEPNESS_MAP = maplib_sip.GeoDrawable.TYPE_STEEPNESS_MAP
    TYPE_LEGEND = maplib_sip.GeoDrawable.TYPE_LEGEND
    TYPE_OVERVIEW = maplib_sip.GeoDrawable.TYPE_OVERVIEW
    TYPE_IMAGE = maplib_sip.GeoDrawable.TYPE_IMAGE
    TYPE_GPSTRACK = maplib_sip.GeoDrawable.TYPE_GPSTRACK
    TYPE_GRIDLINES = maplib_sip.GeoDrawable.TYPE_GRIDLINES
    TYPE_POI_DB = maplib_sip.GeoDrawable.TYPE_POI_DB
    TYPE_ERROR = maplib_sip.GeoDrawable.TYPE_ERROR

    def __init__(self):
        super().__init__()
        self.drawable = None
        self.alternate_views = []

    @property
    def basename(self):
        """Filename without directory component"""

        return os.path.basename(self._fname)

    @property
    def dirname(self):
        """Directory of the FileListEntry"""

        return os.path.dirname(self._fname)

    @property
    def entry_type(self):
        """Type of the object"""

        return self._type

    @property
    def storagename(self):
        """Filename/URL from which the object can be re-generated

        Callers should not try to open the resulting filename, as it may well
        be an URL or an abstract handle.
        """

        return self._fname


class MapFile(FileListEntry):
    def __init__(self, fname):
        super().__init__()
        self.drawable = maplib_sip.LoadMap(fname)
        self.alternate_views = maplib_sip.AlternateMapViews(self.drawable)
        self._storagename = fname
        self._type = self.drawable.GetType()

        self.composite = fname.startswith('composite_map:')
        if self.composite:
            # Set the displayed filename to the something sensible.
            self._fname = self.composite_fname(fname)
        else:
            self._fname = fname

    @property
    def storagename(self):
        return self._storagename

    def _is_punctuation_or_space(self, char):
        """Return True if char is a punctuation or space character

        Works for arbitrary Unicode codepoints, looking for the categories
        P* and Z*.
        """

        # Cf. Unicode categories: www.fileformat.info/info/unicode/category
        return unicodedata.category(char)[0] in 'PZ'

    def composite_fname(self, fname):
        """Generate a sensible dirname/basename for composite maps"""

        fnames = maplib_sip.CompositeMap.ParseFname(fname)[0]
        prefix = os.path.commonprefix(fnames)
        while prefix and self._is_punctuation_or_space(prefix[-1]):
            prefix = prefix[:-1]
        dirname, basename = os.path.split(prefix)
        if not basename:
            dirname, basename = os.path.split(dirname)
        return os.path.join(dirname, _("Composite({})").format(basename))


class GPXFile(FileListEntry):
    def __init__(self, fname):
        super().__init__()
        self._fname = fname
        self._type = FileListEntry.TYPE_GPSTRACK
        self.drawable = maplib_sip.GeoDrawableShPtr(GPSTrack(fname))
        self.alternate_views = []


class POI_Entry:
    def __init__(self, row):
        self.lat, self.lon, self.height, self.category, \
                  self.name, self.district = row

class POI_Database(FileListEntry):
    def __init__(self, fname):
        super().__init__()
        self._fname = fname
        self._type = FileListEntry.TYPE_POI_DB

        # Check if the file exists before calling connect(), as that will
        # happily create a new, empty db.
        if not os.access(fname, os.R_OK):
            raise FileOpenError("Could not access file: '%s'", fname)

        try:
            self.conn = sqlite3.connect(fname)

            # Check right here if we can actually query the DB and if the
            # schema is valid.
            self.conn.execute('SELECT count(*) FROM pois')
        except sqlite3.Error as e:
            raise FileParseError("Not a valid database '%s':\n%s",
                                 fname, str(e)) from None

    def search_name(self, s):
        cur = self.conn.cursor()
        s = '%' + s + '%'
        cur.execute('SELECT * FROM pois WHERE name LIKE ? ORDER BY name ASC',
                    (s,))
        for row in cur:
            yield POI_Entry(row)


class ErrorEntry(FileListEntry):
    def __init__(self, fname, e):
        super().__init__()
        self._fname = fname
        self._type = FileListEntry.TYPE_ERROR
        self.exception = e


class FileList:
    def __init__(self):
        self.maplist = []
        self.gpxlist = []
        self.dblist = []

    def add_file(self, fname, ftype=None):
        if ftype is None:
            if fname.starts_with('composite_map:'):
                ftype = 'MAP'
        if ftype is None:
            ftypes = {'gpx': 'GPX',
                      'db': 'DB',
                      'tif': 'MAP',
                      'tiff': 'MAP',
                      'gvg': 'MAP',
                     }
            ext = fname.lower().rsplit('.', 1)[-1]
            ftype = ftypes.get(ext, None)

        if ftype == "GPX":
            targetlist = self.gpxlist
            cls = GPXFile
        elif ftype == 'DB':
            targetlist = self.dblist
            cls = POI_Database
        elif ftype == 'MAP':
            targetlist = self.maplist
            cls = MapFile
        else:
            raise NotImplementedError("Couldn't detect file type")

        try:
            entry = cls(fname)
        except FileLoadError as e:
            entry = ErrorEntry(fname, e)
        # Take care to store the original ftype for ErrorEntry as well,
        # so we know which list to delete off!
        entry.ftype = ftype
        targetlist.append(entry)

    def store_to(self, store):
        store.set_stringlist('maps',
                [map_entry.storagename for map_entry in self.maplist])
        store.set_stringlist('gpxlist',
                [gpx_entry.storagename for gpx_entry in self.gpxlist])
        store.set_stringlist('dblist',
                [db_entry.storagename for db_entry in self.dblist])

    def retrieve_from(self, store):
        def do_retrieve(ps, name, ftype):
            # Exceptions reading from the persistentstore are likely
            # from running the first time - ignore them.
            try:
                files = ps.get_stringlist(name)
            except KeyError:
                pass
            else:
                for fname in files:
                    self.add_file(fname, ftype=ftype)

        do_retrieve(store, 'maps', 'MAP')
        do_retrieve(store, 'gpxlist', 'GPX')
        do_retrieve(store, 'dblist', 'DB')

    def delete(self, item):
        # Raise ValueError if we can't find item.
        if item.ftype == 'DB':
            self.dblist.remove(item)
        elif item.ftype == 'GPX':
            self.gpxlist.remove(item)
        elif item.ftype == 'MAP':
            self.maplist.remove(item)
        else:
            raise NotImplementedError("Couldn't detect file type")


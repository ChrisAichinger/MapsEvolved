import os
import sqlite3

from . import maplib_sip as maplib_sip
from .errors import MapLibError, FileLoadError, FileOpenError, FileParseError

class POI_Entry:
    def __init__(self, row):
        self.lat, self.lon, self.height, self.category, \
                  self.name, self.district = row

class POI_Database:
    def __init__(self, fname):
        self.fname = fname

        # Check if the file exists before calling connect(), as that will
        # happily create a new, empty db.
        if not os.access(fname, os.R_OK):
            raise FileOpenError("Could not access file: '%s'", fname)

        try:
            self.conn = sqlite3.connect(fname)

            # Check right here if we can actually query the DB and if the
            # schema is valid.
            self.conn.execute('SELECT count(*) FROM pois')
        except sqlite3.Error:
            raise FileParseError("Not a valid database: '%s'", fname)

    def GetType(self):
        return maplib_sip.GeoDrawable.TYPE_POI_DB

    def GetFname(self):
        return self.fname

    def search_name(self, s):
        cur = self.conn.cursor()
        s = '%' + s + '%'
        cur.execute('SELECT * FROM pois WHERE name LIKE ? ORDER BY name ASC',
                    (s,))
        for row in cur:
            yield POI_Entry(row)


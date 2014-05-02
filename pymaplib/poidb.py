import sqlite3

from . import maplib_sip as maplib_sip

class POI_Entry:
    def __init__(self, row):
        self.lat, self.lon, self.height, self.category, \
                  self.name, self.district = row

class POI_Database:
    def __init__(self, fname):
        self.fname = fname
        self.conn = sqlite3.connect(fname)

        # Raise OperationalError if table does not exist (invalid schema)
        self.conn.execute('SELECT count(*) FROM pois')

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


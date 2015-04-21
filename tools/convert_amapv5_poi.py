#!/usr/bin/python3.3

# Requires pyodbc.

# Austrian Map v5 accesses its POI database via ODBC from a library called
# d2rligaz_VC7.dll.
# To intercept calls to SQLConnect() use the following WinDbg command:
#   0:000> bp d2rligaz_VC7+0x4A720 "dda esp L7; g;"
# This displays a stack dump every time a DB connection is established.
# DSN, UID and PWD are visible there.

import sys
import sqlite3

try:
    import pyodbc
except ImportError:
    print("Could not import the pyodbc module\n"
          "To use this script, please install pyodbc.", file=sys.stderr)
    sys.exit(1)

def extract_schema(connectstr, fname):
    conn = pyodbc.connect(connectstr)
    cur = conn.cursor()

    with open(fname, 'w', encoding='utf-8') as f:
        tnames = []
        for t in cur.tables():
            tnames.append(t)

        for t in tnames:
            try:
                count = cur.execute('SELECT COUNT(*) FROM ' + t[2]).fetchone()
            except pyodbc.ProgrammingError:
                count = ('???',)
            print(t[1:], count[0], file=f)
            for c in cur.columns(t[2]):
                print("  ", repr(c[1:]), file=f)

def table_to_file(connectstr, table, fname):
    conn = pyodbc.connect(connectstr)
    cur = conn.cursor()

    with open(fname, 'w', encoding='utf-8') as f:
        for t in cur.execute("SELECT * FROM " + table):
            print(t, file=f)

class ObjectEntry:
    def __init__(self, row):
        # Example:
        # (21, 'Bergnamen', 4,
        #  '0103327.310', 'E',
        #  '473214.194', 'N',
        #  ' ', 'Aggenstein (Reutte)',
        #  21, 0.0, 0.0, 0.0, 0.0, 0.0,
        # 'Reutte', '84', '2214', '7305', ' ', ' ', ' ', ' ', ' ', ' ', 21)
        self.object_id, self.category, _,  \
                lon_dms, lon_ew,           \
                lat_dms, lat_ns,           \
                self.height, self.name,    \
                _, _, _, _, _, _,          \
                self.district, *rest = row

        self.lon = self.parse_dms_ns_ew(lon_dms, lon_ew)
        self.lat = self.parse_dms_ns_ew(lat_dms, lat_ns)
        try:
            self.height = float(self.height)
        except ValueError:
            self.height = 0.0

    @staticmethod
    def parse_dms_ns_ew(dms, nsew):
        dms, sec_fractions = dms.split('.')
        deg, minutes, sec = int(dms[:-4]), int(dms[-4:-2]), int(dms[-2:])
        sec += float('0.' + sec_fractions)

        fractional_deg = deg + minutes / 60 + sec / 3600
        if nsew.upper() in 'SW':
            fractional_deg = -fractional_deg
        return fractional_deg

def create_sqlite_schema(sqlite_cur):
    sqlite_cur.execute("""DROP TABLE IF EXISTS pois""")
    sqlite_cur.execute("""DROP INDEX IF EXISTS poi_name_index""")
    sqlite_cur.execute("""CREATE TABLE pois (
                          lat REAL NOT NULL,
                          lon REAL NOT NULL,
                          height REAL NOT NULL,
                          category TEXT NOT NULL,
                          name TEXT NOT NULL,
                          district TEXT)""")
    sqlite_cur.execute("""CREATE INDEX poi_name_index ON pois (name)""")

def export_to_sqlite(connectstr, sqlite_cur):
    conn = pyodbc.connect(connectstr)
    cur = conn.cursor()

    cur.execute(
        "SELECT * " +
        "FROM {oj OBJECT_LIST LEFT OUTER JOIN OBJECT_LIST_EXTENDED " +
        "         ON OBJECT_LIST.object_id=OBJECT_LIST_EXTENDED.object_id}")
    for t in cur:
        o = ObjectEntry(t)
        sqlite_cur.execute('INSERT INTO pois VALUES (?, ?, ?, ?, ?, ?)',
                           (o.lat, o.lon, o.height, o.category,
                            o.name, o.district))


def main():
    g_usr = "DSN=GEOGRID_USR;UID=DORNIER;PWD=;"
    g_002 = "DSN=GEOGRID_002;UID=DORNIER;PWD=GeogridObjDat;"
    g_003 = "DSN=GEOGRID_003;UID=DORNIER;PWD=GeogridObjDat;"

    conn = sqlite3.connect("poi_amap_v5.db")
    cur = conn.cursor()

    create_sqlite_schema(cur)
    conn.commit()

    export_to_sqlite(g_002, cur)
    export_to_sqlite(g_003, cur)
    conn.commit()
    conn.close()

    # Obsolete code to explore the DB Schema.
    #extract_schema(g_usr, "GEOGRID_SCHEME_USR.txt")
    #extract_schema(g_002, "GEOGRID_SCHEME_002.txt")
    #extract_schema(g_003, "GEOGRID_SCHEME_003.txt")

    #table_to_file(g_002, "OBJECT_LIST", "GEOGRID_002-OL.txt")
    #table_to_file(g_002, "OBJECT_LIST_EXTENDED", "GEOGRID_002-EOL.txt")

    #table_to_file(g_003, "OBJECT_LIST", "GEOGRID_003-OL.txt")
    #table_to_file(g_003, "OBJECT_LIST_EXTENDED", "GEOGRID_003-EOL.txt")

if __name__ == '__main__':
    main()

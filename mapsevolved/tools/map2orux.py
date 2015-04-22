#!/usr/bin/python3

import sys
import os
import io
import string
import sqlite3
import argparse
import contextlib
import collections
import xml.etree.ElementTree as ET

import wx

try:
    import pymaplib
except ImportError:
    sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
    import pymaplib


DEFAULT_TILE_SIZE = pymaplib.MapPixelDeltaInt(512, 512)


TileCounts = collections.namedtuple('TileCounts', ['x', 'y'])


class ExtendedStringFormatter(string.Formatter):
    """A string formatter allowing !i and !f conversions to int/float"""
    def convert_field(self, value, conversion):
        if conversion == 'i' or conversion == 'd':
            return int(value)
        elif conversion == 'f':
            return float(value)
        else:
            return super().convert_field(value, conversion)

def E(tag, *args, **kwargs):
    """XML Element Factory

    Create an XML element. Attributes are taken from all dict objects
    passed, as well as from any keyword args. All other arguments are turned
    into children. They must be either str's or other XML elements.

    Examples:
        >>> ET.tostring(E("tag")).decode('ascii')
        '<tag/>'
        >>> ET.tostring(E("tag", "text")).decode('ascii')
        '<tag>text</tag>'
        >>> ET.tostring(E("tag", "text", key="val")).decode('ascii')
        '<tag key="val">text</tag>'
        >>> ET.tostring(E("tag", {"key": "val"}, "tail", "..")).decode('ascii')
        '<tag key="val">tail..</tag>'
        >>> ET.tostring(E("tag", E("subtag", "text"), "tail")).decode('ascii')
        '<tag><subtag>text</subtag>tail</tag>'
    """

    attrs = dict()
    for arg in args:
        if isinstance(arg, dict):
            attrs.update(arg)
    attrs.update(kwargs)

    # Stringify the attr values, otherwise ET chokes on integers and floats.
    elem = ET.Element(tag, {k: str(v) for k,v in attrs.items()})

    for arg in args:
        if isinstance(arg, dict):
            continue
        if isinstance(arg, str):
            # The string between elem and its first child is elem.text, all
            # strings after that are stored in the .tail of elem's children.
            if not list(elem):
                # Both .text and .tail may be None, ensure we get proper str's.
                elem.text = (elem.text or "") + arg
            else:
                last_child = list(elem)[-1]
                last_child.tail = (last_child.tail or "") + arg
        else:
            elem.append(arg)
    return elem


def get_orux_datum_proj(map):
    """Retrieve an OruxMaps compatible datum and projection from map"""

    map_proj = map.GetProj()
    if not map_proj.IsValid():
        print("The map has no projection information.", file=sys.stderr)
        return None, None

    ps = map_proj.GetProjString().decode('ascii', errors='replace')
    pinfo = dict(s.strip('+ ').split('=') for s in ps.split())

    # Obtain the datum and ellipsoid.
    if "ellps" not in pinfo:
        print("No ellipsoid definition available for map.", file=sys.stderr)
        return None, None

    ellps_lookup = {
        "WGS84": "WGS 1984:Global Definition@WGS 1984:Global Definition",
        "bessel": "Potsdam Rauenberg DHDN:Poland@WGS 1984:Global Definition",
        }
    orux_datum = ellps_lookup.get(pinfo["ellps"])
    if not orux_datum:
        print("Unknown ellipsis: '%s'", pinfo["ellps"], file=sys.stderr)
        return None, None

    # Obtain the projection string.
    # OruxMaps format examples:
    #    "UTM,33" or "Transverse Mercator,12,0,1,4.5E6,0"
    if "proj" not in pinfo:
        print("No projection definition available for map.", file=sys.stderr)
        return None, None

    proj_template = None
    if pinfo["proj"] == "utm":
        proj_template = "UTM,{zone!i}"
    elif pinfo["proj"] == "tmerc":
        proj_template = "Transverse Mercator," + \
                        "{lon_0!i},{lat_0!i},{k!i},{x_0!f},{y_0:f}"
    else:
        print("Unknown projection: '%s'.", pinfo["proj"], file=sys.stderr)
        return None, None

    formatter = ExtendedStringFormatter()
    try:
        orux_projection = formatter.vformat(proj_template, [], pinfo)
    except (KeyError, ValueError):
        print("Failed to obtain required projection parameters.",
              file=sys.stderr)
        return None, None

    return orux_datum, orux_projection

def calc_osm_zoomlevel(map):
    """Get the OpenStreetMap zoom level closest to the map resolution"""

    ok, mpp = pymaplib.MetersPerPixel(map, pymaplib.MapPixelCoord(0,0))
    if not ok:
        print("Map has unknown meters/pixel.", file=sys.stderr)
        return None

    # The cutoff between levels is the geometric middle of their MPP numbers.
    # Cf. http://wiki.openstreetmap.org/wiki/Zoom_levels
    mpp_level0 = 156412
    level = 0
    while True:
        if mpp > mpp_level0 / 2 ** (level + 0.5):
            return level
        level += 1


def scaled_jpeg_tile(map, tx, ty, output_tile_size, scale):
    """Obtain a JPEG compressed tile from a map

    Aside from obtaining tiles from the map and compressing them,
    this function also supports tile rescaling.

    Arguments:
      map: The map to operate on.
      tx, ty: X and Y tile indices, not pixel coordinates!
      output_tile_size: The desired output tile size.
      scale: Act as if map had been scaled by this factor. tx, ty, and
             output_tile_size are in reference to the scaled map.
             Larger scale values mean the map size is "zoomed out" or shrunk
             (less pixel width/height).

    Returns:
      A bytes() object containing the JPEG compressed tile. The compression
      level defaults to 75.

    Example:
      A scale of 2 with an output_tile_size of (512, 512) means this function
      will grab a (1024, 1024) chunk from the map, rescale it down to
      (512, 512), JPEG compress it and return the resulting bytes object.

      If this map has GetSize() == (4096, 2048), valid tx values will be 0..3
      and valid ty values will be 0..1, inclusive.
    """

    # If scale > 1, we need to request a larger area from the map first,
    # then scale it down to the desired output_tile_size.
    scaled_size = pymaplib.MapPixelDeltaInt(output_tile_size.x * scale,
                                            output_tile_size.y * scale)
    coord = pymaplib.MapPixelCoordInt(tx * scaled_size.x, ty * scaled_size.y)
    pb = map.GetRegion(coord, scaled_size)

    # Set alpha values to 255 to work around goofiness in wx.
    data = bytearray(pb.GetData())
    for i in range(pb.GetWidth() * pb.GetHeight()):
        data[4*i + 3] = 0xFF

    # Mirror image vertically - GetRegion() produces the wrong
    # top-down/bottom-up orientation for us.
    bmp = wx.Bitmap.FromBufferRGBA(scaled_size.x, scaled_size.y, data)
    img = bmp.ConvertToImage().Mirror(horizontally=False)
    if scale != 1:
        img.Rescale(output_tile_size.x, output_tile_size.y,
                    wx.IMAGE_QUALITY_BILINEAR)
    bio = io.BytesIO()
    img.SaveFile(bio, wx.BITMAP_TYPE_JPEG)
    return bio.getvalue()


class OruxXML:
    def __init__(self, mapname):
        self.mapname = mapname
        self.datum = None       # An OruxMaps-style datum string
        self.projection = None  # An OruxMaps-style projection string

        self._reslevels = []
        self._xmltree = None

    def add_reslevel(self, zoomlevel, img_size, tile_size, num_tiles,
                     TL, TR, BR, BL):
        """Add metadata for one resolution level

        Add metadata for one resolution level to the internal reslevel list.

        Arguments:
         - zoomlevel: The OSM zoom level for this resolution.
         - img_size, tile_size: The image and tile size in pixels.
           Both objects must support .x and .y variable/property access.
         - num_tiles: The number of tiles in x and y direction.
           Must be an object that supports .x and .y variable/property access.
         - TL, TR, BR, BL: LatLon coordinates of the map corners.
        """

        # datum and projection must be set before adding the first reslevels.
        assert self.datum and self.projection

        corners = [TL, TR, BR, BL]
        ll_min = pymaplib.LatLon(min(ll.lat for ll in corners),
                                 min(ll.lon for ll in corners))
        ll_max = pymaplib.LatLon(max(ll.lat for ll in corners),
                                 max(ll.lon for ll in corners))
        self._reslevels.append(
            E("OruxTracker", {"versionCode": "2.1"},
              E("MapCalibration", {"layers": "false", "layerLevel": zoomlevel},
                E("MapName", "%s %d" % (self.mapname, zoomlevel)),
                E("MapChunks", xMax=num_tiles.x, yMax=num_tiles.y,
                               datum=self.datum, projection=self.projection,
                               img_height=tile_size.y, img_width=tile_size.x,
                               file_name="%s %d" % (self.mapname, zoomlevel)),
                E("MapDimensions", width=img_size.x, height=img_size.y),
                E("MapBounds", minLat=ll_min.lat, maxLat=ll_max.lat,
                               minLon=ll_min.lon, maxLon=ll_max.lon),
                E("CalibrationPoints",
                  # OruxMaps creates this corner order, maintain compatibility.
                  E("CalibrationPoint", corner="TL", lon=TL.lon, lat=TL.lat),
                  E("CalibrationPoint", corner="BR", lon=BR.lon, lat=BR.lat),
                  E("CalibrationPoint", corner="TR", lon=TR.lon, lat=TR.lat),
                  E("CalibrationPoint", corner="BL", lon=BL.lon, lat=BL.lat)
                )
              )
            ))

    def _finalize_xml(self):
        """Produce the final OruxMaps XML tree

        Wrap the added reslevels more metadata to produce the final output XML
        tree in OruxMaps / OTRK2 XML format.
        """

        orux_ns = "http://oruxtracker.com/app/res/calibration"
        self._xmltree = \
                E("OruxTracker", {"xmlns:orux": orux_ns, "versionCode": "3.0"},
                  E("MapCalibration", {"layers": "true", "layerLevel": 0},
                    E("MapName", self.mapname),
                    *self._reslevels
                  ))

    def save_to(self, f):
        """Save the generated XML tree to a file

        The argument f can either be a filename or a file object.
        """

        if isinstance(f, str):
            with open(f, 'wb') as fileobj:
                return self.save_to(fileobj)
        self._finalize_xml()
        xml_bytes = self.xmltostring(self._xmltree, "UTF-8",
                                     xml_declaration=True)
        f.write(xml_bytes)

    @staticmethod
    def xmltostring(element, encoding=None, xml_declaration=None, method=None):
        """Convert an XML tree to a string representation

        This mimics ET.toxml(), but supports the xml_declaration argument that
        we need to generate OruxMaps XML files.
        """

        stream = io.StringIO() if encoding == 'unicode' else io.BytesIO()
        ET.ElementTree(element).write(stream, encoding,
                                      xml_declaration=xml_declaration,
                                      method=method)
        return stream.getvalue()


# The OruxMaps OTRK2 DB is an Sqlite file containing JPEG tiles, indexed by
# x, y, z (zoom level) indices. x == y == 0 is the top-left tile of the map,
# x increases to the right, y to the bottom. The available zoom levels, as well
# as maximum x/y indices for each level are obtained from the otrk2.xml file.
class OruxTileDatabase:
    """Access the tile database of OruxMap maps

    Allow loading and storing tiles from/to OruxMapImages.db databases.
    Tiles are addressed by a ``(x, y, z)`` triplet. ``x`` and ``y`` are tile
    indices into the map, ``z`` is an OpenStreetMap zoom level.

    This class can be used as s context manager to ensure changes are committed
    to the data base.
    """

    def __init__(self, fname):
        self.fname = fname
        self.conn = sqlite3.connect(fname)

    def commit(self):
        self.conn.commit()

    def close(self):
        self.conn.close()

    def create_schema(self):
        """Delete any existing tables and re-create the schema from scratch"""

        # Explicitly call commit() so users don't have to bother using
        # a with statement here. It's not performance critical.
        c = self.conn.cursor()
        c.execute('DROP TABLE IF EXISTS tiles')
        c.execute('DROP INDEX IF EXISTS IND')
        c.execute('DROP TABLE IF EXISTS android_metadata')
        self.conn.commit()

        c.execute('''CREATE TABLE tiles (x INT, y INT, z INT, image BLOB,
                                         PRIMARY KEY (x,y,z))''')
        c.execute('CREATE INDEX IND on tiles (x,y,z)')
        c.execute('CREATE TABLE android_metadata (locale TEXT)')
        c.execute('INSERT INTO android_metadata VALUES ("de_DE")')
        self.conn.commit()

    def all_tile_triplets(self):
        """A generator producing the (x, y, z) triplets of all stored tiles"""

        c = self.conn.cursor()
        c.execute('SELECT x, y, z FROM tiles')
        yield from c

    def load_compressed_tile(self, x, y, z):
        """Load one tile from the database

        Return a bytes object containing the raw, compressed JPEG data.
        """

        c = self.conn.cursor()
        c.execute('SELECT image FROM tiles WHERE x = ? and y = ? and z = ?',
                  (x, y, z))
        return c.fetchone()[0]

    def dump_all_tiles(self, limit=None):
        """Save all tiles as jpeg files in the current directory

        The files will be named "<x>-<y>-<z>.jpg".
        """

        c = conn.cursor()
        if limit is not None:
            c.execute('SELECT * FROM tiles')
        else:
            c.execute('SELECT * FROM tiles LIMIT ?', (limit,))
        for row in c:
            x, y, z, image = row
            fname = '%d-%d-%d.jpg' % (x, y, z)
            with open(fname, 'wb') as f:
                f.write(image)

    def insert_tile(self, x, y, z, image):
        """Insert a tile into the database

        This function does not commit its changes, so either commit() has to be
        called manually, or a ``with db:`` context manager has to be used.
        """

        c = self.conn.cursor()
        c.execute('INSERT INTO tiles VALUES (?,?,?,?)', (x, y, z, image))

    # Make this class a context manager that guarantees a database commit
    # after the with-statement body.
    def __enter__(self):
        pass
    def __exit__(self, exc_type, exc_value, traceback):
        self.conn.commit()


class OruxMapCreator:
    """Convenience class tying together OruxXML and OruxTileDatabase classes

    To produce an OruxMaps map directory:

        >>> oruxmap = OruxMapCreator(<mapname>)
        >>> oruxmap.metadata.datum = <datum string>
        >>> oruxmap.metadata.projection = <projection string>
        >>> for level in reslevels:
        >>>     oruxmap.metadata.add_reslevel(...)
        >>>     oruxmap.tiledb.insert_tile(...)
        >>> oruxmap.save()
    """

    def __init__(self, output_dirname):
        if not os.path.isdir(output_dirname):
            os.mkdir(output_dirname)

        basename = os.path.basename(output_dirname)
        self._xml_fname = os.path.join(output_dirname, basename + ".otrk2.xml")
        self._db_fname = os.path.join(output_dirname, "OruxMapsImages.db")

        self.metadata = OruxXML(basename)
        self.tiledb = OruxTileDatabase(self._db_fname)
        self.tiledb.create_schema()

    def save(self):
        self.tiledb.commit()
        self.metadata.save_to(self._xml_fname)


def convert_to_orux(map, output_name, num_reslevels):
    """Convert map into OruxMaps format

    Convert map to OruxMaps (OTRK2) format and save the result in the directory
    output_name. The argument num_reslevels is the number of resolution levels
    (scaled layers) to include.

    With num_reslevels=1, only the best availble resolution is included.
    At num_reslevels=2, two layers with 100% and 50% resolution are
    generated, num_reslevels=3 produces 100%, 50%, and 25% layers, and
    so on.
    """

    datum, proj = get_orux_datum_proj(map)
    if not datum or not proj:
        return False

    map_width, map_height = map.GetWidth(), map.GetHeight()
    ok1, TL = map.PixelToLatLon(pymaplib.MapPixelCoord(0, 0))
    ok2, BL = map.PixelToLatLon(pymaplib.MapPixelCoord(0, map_height))
    ok3, BR = map.PixelToLatLon(pymaplib.MapPixelCoord(map_width, map_height))
    ok4, TR = map.PixelToLatLon(pymaplib.MapPixelCoord(map_width, 0))
    if not (ok1 and ok2 and ok3 and ok4):
        print("Failed to get corner coordinates of the map.", file=sys.stderr)
        return False

    best_zoomlevel = calc_osm_zoomlevel(map)
    if best_zoomlevel is None:
        return False

    try:
        oruxmap = OruxMapCreator(output_name)
    except OSError as exc:
        print("Failed to create output files: %s." % exc.strerror)
        fname = exc.getattr(exc, 'filename', None)
        if fname is not None:
            print("While trying to access '%s'." % fname)

    oruxmap.metadata.datum = datum
    oruxmap.metadata.projection = proj

    tile_size = DEFAULT_TILE_SIZE
    for i in range(num_reslevels):
        # Each zoom level has half the scale as the previous. Calculate the
        # scaled image size for each level and derive the number of tiles.
        scaled_image_size = pymaplib.MapPixelDeltaInt(map_width // 2**i,
                                                      map_height // 2**i)
        num_tiles = TileCounts(
                  (scaled_image_size.x + tile_size.x - 1) // tile_size.x,
                  (scaled_image_size.y + tile_size.y - 1) // tile_size.y)

        zoomlevel = best_zoomlevel - i
        oruxmap.metadata.add_reslevel(zoomlevel, scaled_image_size, tile_size,
                                      num_tiles, TL, TR, BR, BL)

        # Use the tiledb as context manager to ensure a commit() at the end.
        with oruxmap.tiledb:
            for ty in range(num_tiles.y):
                for tx in range(num_tiles.x):
                    tile = scaled_jpeg_tile(map, tx, ty, tile_size, scale=2**i)
                    oruxmap.tiledb.insert_tile(tx, ty, zoomlevel, tile)

    oruxmap.save()


def main():
    parser = argparse.ArgumentParser(
            description='Convert maps to OruxMaps (OTRK2) format.')
    parser.add_argument('fnames', metavar='input_map', nargs='+',
            help='input maps to convert')
    parser.add_argument('-o', dest="output", metavar="output_name",
            help='output path name')
    parser.add_argument('-p', default="", dest="prefix",
            metavar="output_prefix",
            help='output prefix when processing multiple files')
    parser.add_argument('-l', default=1, type=int, dest="num_reslevels",
            metavar="num_reslevels",
            help='number of resolution levels (zoom steps) to include')
    parser.add_argument('-t', action='store_true', dest="title_name",
            help='include the map title in the output filename ' +
                 '(useful with -p)')
    args = parser.parse_args()

    if args.output and args.prefix:
        print("The options -o and -p are mutually exclusive.",
              file=sys.stderr)
        sys.exit(1)
    if args.output and args.title_name:
        print("The options -o and -t are mutually exclusive.",
              file=sys.stderr)
        sys.exit(1)
    if args.output and len(args.fnames) > 1:
        print("The -o option is not supported when converting multiple maps.",
              file=sys.stderr)
        sys.exit(1)

    for fname in args.fnames:
        map = pymaplib.LoadMap(fname)

        if args.output:
            output_dir = args.output
        else:
            dirname, basename = os.path.split(fname)
            baseroot, ext = os.path.splitext(basename)
            if args.title_name:
                baseroot = map.GetTitle()
            output_dir = os.path.join(dirname, args.prefix + baseroot)

        convert_to_orux(map, output_dir, args.num_reslevels)

if __name__ == '__main__':
    main()

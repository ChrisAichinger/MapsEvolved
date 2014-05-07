import math
import itertools

from . import maplib_sip
from .maplib_sip import MapPixelCoord, MapPixelCoordInt


class CombinableTile:
    """Container for a single map that may be combined into a CompositeMap

    This class represents one single map that can potentially be paired with
    others to produce a CompositeMap. Its interface is centered around
    checking whether it matches another map and tracking fitting "neighbors".
    """

    def __init__(self, container):
        self.container = container
        self.map = container.drawable
        self.size = size = self.map.GetSize()

        # Our neighbors: left, right, top, bottom.
        self.l = self.r = self.t = self.b = None

        # The LatLon coordinates of our 4 corners.
        ok1, self.tl = self.map.PixelToLatLon(MapPixelCoord(0, 0))
        ok2, self.tr = self.map.PixelToLatLon(MapPixelCoord(size.x, 0))
        ok3, self.bl = self.map.PixelToLatLon(MapPixelCoord(0, size.y))
        ok4, self.br = self.map.PixelToLatLon(MapPixelCoord(size.x, size.y))
        if not (ok1 and ok2 and ok3 and ok4):
            raise RuntimeError("Failed to get map coordinates")

        # Roughly estimate degrees/pixel to get an epsilon for later
        # coordinate equality checking.
        size_lat = self.tl.lat - self.tr.lat
        size_lon = self.tl.lon - self.tr.lon
        degrees = math.sqrt(size_lat**2 + size_lon**2)
        self.deg_per_pix = degrees / size.x

        call_ok, self.mpp = maplib_sip.MetersPerPixel(container.drawable,
                                                      MapPixelCoordInt(0, 0))
        if not call_ok:
            raise RuntimeError("Failed to determine meters/pixel for map")

    def equal(self, latlon1, latlon2):
        """Check if two LatLon coordinates are equal

        We consider them equal if their distance is less than (roughly) one
        map pixel.
        """
        return (abs(latlon1.lat - latlon2.lat) < self.deg_per_pix and
                abs(latlon1.lon - latlon2.lon) < self.deg_per_pix)

    def do_pair_with(self, other, self_direction):
        """Pair two maps by making them neighbors of each other

        Make ``other`` a neighbor of ``self`` to the side ``self_direction``.
        Also update ``other`` so that ``self`` is its neighbor in the opposite
        direction.
        """

        if self_direction == 'l':
            if self.l or other.r:
                return
            self.l = other
            other.r = self
        elif self_direction == 'r':
            if self.r or other.l:
                return
            self.r = other
            other.l = self
        elif self_direction == 't':
            if self.t or other.b:
                return
            self.t = other
            other.b = self
        elif self_direction == 'b':
            if self.b or other.t:
                return
            self.b = other
            other.t = self

    def try_pair_with(self, other):
        """Try to make ``other`` a neighbor of this object

        Test if the maps have compatible scale (meters per pixel), then test
        if they can be paired along any of their four sides. This is the case
        if their corner coordinates (LatLon) along that side are equal.
        Finally, ensure they have the same number of pixels along the side
        where they are to be joined.

        If they are pairable, pair them using do_pair_with().

        This operational is symmetrical regarding self and other:
            self.try_pair_with(other) is the same as other.try_pair_with(self)
        """

        # Only merge maps if their MPP are within 1% of each other.
        mpp_delta = (self.mpp - other.mpp) / self.mpp
        if abs(mpp_delta) > 0.01:
            return

        if self.equal(self.tl, other.tr) and self.equal(self.bl, other.br):
            if self.size.y == other.size.y:
                self.do_pair_with(other, 'l')
        elif self.equal(self.tr, other.tl) and self.equal(self.br, other.bl):
            if self.size.y == other.size.y:
                self.do_pair_with(other, 'r')
        elif self.equal(self.tl, other.bl) and self.equal(self.tr, other.br):
            if self.size.x == other.size.x:
                self.do_pair_with(other, 't')
        elif self.equal(self.bl, other.tl) and self.equal(self.br, other.tr):
            if self.size.x == other.size.x:
                self.do_pair_with(other, 'b')

    def iter_right(self):
        """Iterate over the neighbors to the right (including self)"""

        map = self
        while map:
            yield map
            map = map.r

    def iter_down(self):
        """Iterate over the neighbors below (including self)"""

        map = self
        while map:
            yield map
            map = map.b

class CompositableTiles:
    """Collection of tiles eligable for creating a CompositeMap

    To create CompositableTiles, use the find_composities() function.
    """
    def __init__(self, raster):
        self.num_x = len(raster)
        self.num_y = len(raster[0])
        self.tiles = raster

        self.tile_maps = [None] * self.num_x * self.num_y
        self.tile_containers = [None] * self.num_x * self.num_y
        for x, col in enumerate(self.tiles):
            for y, tile in enumerate(col):
                self.tile_maps[x + y * self.num_x] = tile.map
                self.tile_containers[x + y * self.num_x] = tile.container

    def get_composite_map_fname(self):
        return maplib_sip.CompositeMap.FormatFname(
                self.num_x, self.num_y, self.tile_maps)

def find_composites(maplist):
    """A generator producing CompositableTiles objects from a maplist"""

    # Sort maps into buckets with the same directory, type and projection.
    buckets = {}
    for container in maplist:
        if not container.drawable:
            continue
        key = (container.dirname,
               container.drawable.GetType(),
               container.drawable.GetProj().GetProjString())
        buckets.setdefault(key, []).append(container)

    for containers in buckets.values():
        if len(containers) < 2:
            # Can't create a composite of one single map.
            continue

        tiles = [CombinableTile(c) for c in containers]
        for tile1, tile2 in itertools.combinations(tiles, 2):
            # Iterate over all pairs of tiles.
            tile1.try_pair_with(tile2)

        # Find the topleft tiles.
        tl_tiles = [tile for tile in tiles
                    if tile.b and tile.r and not tile.l and not tile.t]

        # Each topleft tile is converted to a 2D array of tiles.
        rasters = (_raster_from_topleft_tile(tl_tile) for tl_tile in tl_tiles)
        for raster in rasters:
            # Discard failed tile-to-raster conversions.
            if raster:
                yield CompositableTiles(raster)

def _raster_from_topleft_tile(tl_tile):
    """Take a top-left tile and produce a rectangular 2D array of tiles

    Start from ``tl_tile`` and trace the graph generated by the neighbor
    links (l/r/t/b). Create a rectangular 2D list-of-lists of the nodes.

    The result may be None if the tile graph is malformed, for example if
    it doesn't form a rectangle.

    Example result:
        [[tile1, tile2],
         [tile3, tile4]]
    """

    num_tiles_x = len(list(tl_tile.iter_right()))
    num_tiles_y = len(list(tl_tile.iter_down()))

    raster = [[None] * num_tiles_y for i in range(num_tiles_x)]
    try:
        # Populate the tile raster down-right.
        for k, leftborder_tile in enumerate(tl_tile.iter_down()):
            if leftborder_tile.l:
                # If we have an "outgrowing" tile to the left, abort.
                return None
            for j, tile in enumerate(leftborder_tile.iter_right()):
                raster[j][k] = tile
        # Ensure it is the same when viewed right-down (consistency check).
        for j, topborder_tile in enumerate(tl_tile.iter_right()):
            if topborder_tile.t:
                # If we have an "outgrowing" tile to the top, abort.
                return None
            for k, tile in enumerate(topborder_tile.iter_down()):
                if raster[j][k] != tile:
                    return None
        # We don't need to consistency-check up-left or left-up, as we
        # treat l/r and t/b symmetrically in CombinableTile.

    except IndexError:
        # If j >= num_tiles_x or k >= num_tiles_y we land here.
        # This means the raster is not rectangular, so abort.
        return None

    # If any None entries are left in the raster, abort as well.
    # This complements the IndexError check above.
    for row in raster:
        if not all(row):
            return None

    return raster

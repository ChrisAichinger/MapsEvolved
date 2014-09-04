import math
import itertools

from . import maplib_sip
from .maplib_sip import MapPixelCoord, MapPixelDeltaInt


class CombinableTile:
    """Container for a single map that may be combined into a CompositeMap

    This class represents one single map that can potentially be paired with
    others to produce a CompositeMap. Its interface is centered around
    checking whether it matches another map and tracking fitting "neighbors".
    """

    def __init__(self, container, has_overlap_pixel):
        self.container = container
        self.map = container.drawable
        self.has_overlap_pixel = has_overlap_pixel
        self.neighbors = {side: None for side in 'lrtb'}

        size = self.map.GetSize()
        if has_overlap_pixel:
            size -= MapPixelDeltaInt(1, 1)

        corners = {"tl": MapPixelCoord(0, 0),
                   "tr": MapPixelCoord(size.x, 0),
                   "bl": MapPixelCoord(0, size.y),
                   "br": MapPixelCoord(size.x, size.y),
                   }
        self.latlon = dict()
        self.mpp = dict()
        for name, coord in corners.items():
            ok, self.latlon[name] = self.map.PixelToLatLon(coord)
            if not ok:
                raise RuntimeError("Failed to get map corner coordinates")
            ok, self.mpp[name] = maplib_sip.MetersPerPixel(
                                       container.drawable, coord)
            if not ok:
                raise RuntimeError("Failed to determine meters/pixel for map")

        # Roughly estimate degrees/pixel to get an epsilon for later
        # coordinate equality checking.
        size_lat = self.latlon["tl"].lat - self.latlon["br"].lat
        size_lon = self.latlon["tl"].lon - self.latlon["br"].lon
        diagonal_deg = math.sqrt(size_lat**2 + size_lon**2)
        diagonal_pix = math.sqrt(corners["br"].x**2 + corners["br"].y**2)
        self.deg_per_pix = diagonal_deg / diagonal_pix

    def ll_equal(self, latlon1, latlon2):
        """Check if two LatLon coordinates are equal

        We consider them equal if their distance is less than (roughly) half a
        map pixel.
        """
        return (abs(latlon1.lat - latlon2.lat) < 0.5 * self.deg_per_pix and
                abs(latlon1.lon - latlon2.lon) < 0.5 * self.deg_per_pix)

    def mpp_equal(self, mpp1, mpp2):
        """Check if two meters/pixel values are equal

        We consider them equal, if they are within 1% of each other.
        """

        average_mpp = (mpp1 + mpp2) / 2
        mpp_delta = (mpp1 - mpp2) / average_mpp
        return abs(mpp_delta) <= 0.01

    def is_side_compatible(self, other, side):
        """Check if this map can be paired with another one on a given side

        Ensure the corner coordinates are close enough in the LatLon system,
        that the resolution on the corners is similar, and that the maps
        have the same number of pixels along the relevant side.

        The ``side`` argument is seen from self, e.g. side='l' means the left
        side of self will be paired with the right side of other.
        """

        corner_pairs = {
                # Sides and what corners have to contact for the pair
                # to match.
                'l': [('tl', 'tr'), ('bl', 'br')],
                'r': [('tr', 'tl'), ('br', 'bl')],
                't': [('tl', 'bl'), ('tr', 'br')],
                'b': [('bl', 'tl'), ('br', 'tr')],
                }
        (A_self, A_other), (B_self, B_other) = corner_pairs[side]
        if not self.ll_equal(self.latlon[A_self], other.latlon[A_other]):
            return False
        if not self.mpp_equal(self.mpp[A_self], other.mpp[A_other]):
            return False

        if not self.ll_equal(self.latlon[B_self], other.latlon[B_other]):
            return False
        if not self.mpp_equal(self.mpp[B_self], other.mpp[B_other]):
            return False

        if side in 'lr':
            return self.map.GetHeight() == other.map.GetHeight()
        else:
            return self.map.GetWidth() == other.map.GetWidth()

    def do_pair_with(self, other, side_self):
        """Pair two maps by making them neighbors of each other

        Make ``other`` a neighbor of ``self`` to the side ``side_self``.
        Also update ``other`` so that ``self`` is its neighbor on the opposite
        side.
        """

        side_other = dict(zip('lrtb', 'rlbt'))[side_self]
        if self.neighbors[side_self] or other.neighbors[side_other]:
            return
        self.neighbors[side_self] = other
        other.neighbors[side_other] = self

    def try_pair_with(self, other):
        """Try to make ``other`` a neighbor of this object

        Try to combine the two maps along each of their four sides.
        If one of them fits, mark them as neighbors along that side.

        This operational is symmetrical regarding self and other:
            self.try_pair_with(other) is the same as other.try_pair_with(self)
        """

        for side in 'lrtb':
            if self.is_side_compatible(other, side):
                self.do_pair_with(other, side)

    def iter_right(self):
        """Iterate over the neighbors to the right (including self)"""

        map = self
        while map:
            yield map
            map = map.neighbors['r']

    def iter_down(self):
        """Iterate over the neighbors below (including self)"""

        map = self
        while map:
            yield map
            map = map.neighbors['b']


class CompositableTiles:
    """Collection of tiles eligable for creating a CompositeMap

    To create CompositableTiles, use the from_topleft_tile() classmethod.
    """
    def __init__(self, raster):
        self.num_x = len(raster)
        self.num_y = len(raster[0])
        self.tiles = raster
        self.has_overlap_pixel = raster[0][0].has_overlap_pixel

        self.tile_maps = [None] * self.num_x * self.num_y
        self.tile_containers = [None] * self.num_x * self.num_y
        for x, col in enumerate(self.tiles):
            for y, tile in enumerate(col):
                self.tile_maps[x + y * self.num_x] = tile.map
                self.tile_containers[x + y * self.num_x] = tile.container

    def get_composite_map_fname(self):
        return maplib_sip.CompositeMap.FormatFname(
                self.num_x, self.num_y, self.has_overlap_pixel, self.tile_maps)

    @classmethod
    def from_tile_graph(cls, topleft_tile):
        """Create an instance from the top-left tile of a tile graph

        Take the top-left tile of a tile graph, place the nodes into a
        rectangular 2D array. Then, create a CompositableTiles object from
        the array.

        Return the created CompositableTiles object or None if the tile graph
        was malformed, for example if it didn't form a rectangle.

        Example input (tile1 is topleft_tile):
            tile1.neighbors['r'] = tile2, tile1.neighbors['b'] = tile3
            tile2.neighbors['l'] = tile1, tile2.neighbors['b'] = tile4
            tile3.neighbors['r'] = tile4, tile3.neighbors['t'] = tile1
            tile4.neighbors['l'] = tile3, tile4.neighbors['t'] = tile2
        Visualization:
            tile1 <-> tile2
              |         |
            tile3 <-> tile4
        Rasterized result:
            rester[0][0] = tile1, raster[1][0] = tile2
            rester[0][1] = tile3, raster[1][1] = tile4
        """

        raster = [list(t for t in left_border_tile.iter_down())
                  for left_border_tile in topleft_tile.iter_right()]

        # Ensure all sublists have the same length.
        if len(set(len(l) for l in raster)) != 1:
            return None

        # Number of elements in raster when indexed as raster[i][j]
        # i increases from left to right, j from top to bottom.
        num_tiles_x = len(raster)
        num_tiles_y = len(raster[0])
        for x in range(num_tiles_x):
            for y in range(num_tiles_y):
                try:
                    expected = {
                          "l": None if x == 0             else raster[x-1][y],
                          "r": None if x == num_tiles_x-1 else raster[x+1][y],
                          "t": None if y == 0             else raster[x][y-1],
                          "b": None if y == num_tiles_y-1 else raster[x][y+1],
                        }
                    for side in 'lrtb':
                        if raster[x][y].neighbors[side] != expected[side]:
                            return None
                except IndexError:
                    return None
        return cls(raster)


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

    # Overlap pixel: deal with tiles that overlap adjacent tiles by one pixel.
    # See the comment in map_composite.h for more information.
    for has_overlap_pixel in (True, False):
        for containers in buckets.values():
            if len(containers) < 2:
                continue

            # Iterate over all pairs of tiles and link the fitting ones
            # together.
            tiles = [CombinableTile(c, has_overlap_pixel) for c in containers]
            for tile1, tile2 in itertools.combinations(tiles, 2):
                tile1.try_pair_with(tile2)

            for tile in tiles:
                # Find the top-left tiles and place them into a 2D raster.
                if not (tile.neighbors['b'] or tile.neighbors['r']):
                    continue
                if tile.neighbors['l'] or tile.neighbors['t']:
                    continue

                ctiles = CompositableTiles.from_tile_graph(tile)
                if ctiles:
                    yield ctiles

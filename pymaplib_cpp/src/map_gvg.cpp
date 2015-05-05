#include "map_gvg.h"

// Ensure that <cmath> defines M_PI. Keep before other includes.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <limits>
#include <regex>
#include <sstream>

#include "memjpeg.h"
#include "util.h"

// GVG maps consist of two files:
// * 13.gvg: Geospatial and other metadata.
// * 13.gmp: Image data, a tile-based variation of the BMP file format.
// GMP files can exist on their own without the GVG, e.g. for logos or legends.
// GMP files can contain multiple images, which is used to store multiple
// resolutions of the same map, to decrease loading times at low zoom levels.
//
// GVG format:
// GVG files are protected by a simple substitution cypher. After decryption,
// they are latin-1 (ISO-8859-1) encoded INI files. They contain one [Header]
// section along with one or more [MAP1], [MAP2], [MAP4], ... sections.
// * The [Header] section (represented by class GVGHeader) contains data like
//   map title and description.
// * Each [MAP*] section (represented by GVGMapInfo) describes one zoom level
//   of the map.
//   Most important parameters:
//   - Path: "Path=<gmpname>|<index>", e.g. "Path=13.gmp|0"
//     The name of the GMP file along with the index of this resolution within
//     the GMP (multiple resolution images are stored in one GMP).
//   - Ellipsoid: "Ellipsoid=wgs84", "Ellipsoid=bessel", ...
//   - Projection: "Projection=utm", "Projection=gk", ...
//   - BaseMed and Zone:
//     Further defining the projection (e.g. UTM zone, base meridian).
//   - OffsetEast, OffsetNorth: e.g. "OffsetEast=500000.00000"
//     Unsure of their actual meaning, they don't enter into the calculations.
//     If (OffsetEast == 0.0), a special case is invoked, though:
//     500000.0 (5.0E5) is added to OffsetEast and WorldOrgX (see below).
//     OffsetNorth is not special-cased in any way, nor is WorldOrg[XY]
//     modified if OffsetEast is != zero.
//   - WorldOrgX, WorldOrgY:
//     Projected coordinates of the bottom-right map corner.
//   - WPPX, WPPY, RADX, RADY:
//     Those 4, together with WorldOrg[XY] define an affine transformation,
//     mapping pixel coordinates to projected coordinates. RADX and RADY are
//     rotation angles in degrees, WPPX and WPPY are scaling factors.
//     Given PX being a column vector containing (x/y) pixel coordinates,
//     the PCS vector is obtained as:
//           (WPPX     0)   (cos(RADX)  -sin(RADX))        (WorldOrgX)
//     PCS = (   0  WPPY) * (sin(RADY)   cos(RADY)) * PX + (WorldOrgY)
//
//     Since, typically, RADX == RADY, this corresponds to a rotation by RADX
//     degrees, followed by scaling with WPPX and WPPY along the projected
//     coordinate system x/y directions, respectively. Finally, WorldOrg[XY]
//     is added.
//   - ImageWidth/ImageHeight:
//     Can be used to decide which [MAP*] section has the best resolution data.
//
// GMP Format:
// One GMP file can contain multiple images (represented by class GMPImage).
// They are simply concatenated together, there does not seem to be a central
// index. To find the later images, one has to parse the first and find its
// end.
// GMP Image:
// * BITMAPFILEHEADER (14 bytes): ``START + 0x00``
//     - bfType: 0x5847 ("GX")
//     - bfOffBits: Allows calculating the start of the GMPTileOffset table.
// * BITMAPINFOHEADER (40 bytes): Follows the BFH, ``START + 0x0E``
//     - biSize: Allows calculating the start of the GMPHeader.
//     - biHeight: Might be negative to indicate top-down images.
// * GMPHeader (24 bytes): Follows the BIH, ``START + 0x0E + biSize``
//     - compression: compression method
//         4: JPEG compression
//     - tile_px_x, tile_px_y: X and Y tile sizes in pixels.
//
//   The actual map width and hight is NOT given directly by biWidth and
//   biHeight. Instead, it is obtained by rounding biWidth/biHeight up to a
//   tile boundary!
// * GMPTileOffset table: ``START + bfOffBits``
//   An array of GMPTileOffset objects. The number of entries is the total
//   number of tiles: num_tiles_x * num_tiles_y (which can be calculated from
//   the image and tile sizes).
//   - offset: Position of the tile within the file, based off the start of the
//     GMPTileOffset table.
//   - length: Compressed tile size.
//
//   If (offset == -1LL && length == 0), the tile is missing. This can happen
//   at the border of images. It's probably best filled with GVGHeader.BkColor.
//
// * The tile buffers: ``START + bfOffBits + toff.offset``
//   Encrypted with the same substitution cypher as GVG files. The offset into
//   the dectyption table is determined from toff.length.
//   In the case of JPEG images, the decrypted buffers can be saved directly to
//   a file and viewed, or be decompressed from memory.
//   Typically, the red and blue channels are switched, though,
//   this has to be rectified manually.
//
// * The end of the current GMP Image (and start of the next Image) can be
//   found as:
//
//       max(toff.offset + toff.length for toff in tile_offset_table)



static_assert(sizeof(GMPBitmapFileHdr) == 14,
              "GMPBitmapFileHdr has the wrong size.");
static_assert(sizeof(GMPBitmapInfoHdr) == 40,
              "GMPBitmapInfoHdr has the wrong size.");
static_assert(sizeof(GMPHeader) == 24,
              "GMPHeader has the wrong size.");
static_assert(sizeof(GMPTileOffset) == 16,
              "GMPTileOffset has the wrong size.");


// After decryption, GVG files are latin1 (ISO-8859-1) encoded text files.
static const char * const GVG_ENCODING = "ISO-8859-1";

// If the OffsetEast field is not set for a map, force it to 500000.
// This matches the behavior of the official viewers.
static const double DEFAULT_OFFSET_EAST = 500000.0;

// A lookup table to decrypt the GVG files.
static const unsigned char cypher_table[8][256] = {
    { 0x00, 0x06, 0x10, 0x01, 0x02, 0x0b, 0x0c, 0x09, 0x17, 0x05, 0x08, 0x0a,
      0x14, 0x07, 0x03, 0x16, 0x0e, 0x12, 0x18, 0x0d, 0x1d, 0x19, 0x0f, 0x15,
      0x11, 0x1b, 0x13, 0x1c, 0x1a, 0x1e, 0x1f, 0x04, 0x99, 0x77, 0xfd, 0xf8,
      0x23, 0x4e, 0xb3, 0xfe, 0x21, 0xec, 0xa7, 0x75, 0x71, 0xc4, 0xf2, 0xc3,
      0xa8, 0xee, 0x5e, 0xda, 0x34, 0x2b, 0xb4, 0xf1, 0xfb, 0xab, 0xfc, 0x86,
      0x4c, 0x49, 0xfa, 0x44, 0xef, 0x84, 0x35, 0x53, 0xb1, 0x63, 0xe4, 0xdf,
      0x5c, 0x90, 0xc5, 0xf7, 0xb2, 0xb8, 0xa1, 0x48, 0x9a, 0x87, 0x50, 0x83,
      0x7f, 0x38, 0x76, 0x7d, 0x28, 0xaa, 0x9c, 0x6d, 0x3a, 0x95, 0xaf, 0xeb,
      0x79, 0x31, 0xbe, 0xd2, 0x9e, 0xe0, 0x5a, 0xf0, 0x60, 0xbf, 0x68, 0x94,
      0x45, 0xe9, 0x9f, 0x64, 0xb6, 0x3e, 0x2f, 0x78, 0xc7, 0xed, 0x8c, 0x7b,
      0xc9, 0x70, 0xb0, 0xb7, 0xff, 0xcd, 0xde, 0xe3, 0x27, 0x67, 0xa2, 0xbc,
      0x74, 0x97, 0x9b, 0x43, 0xd3, 0x4f, 0xca, 0x2c, 0x9d, 0x4d, 0xf3, 0xd5,
      0x81, 0x62, 0xc1, 0xa4, 0xd4, 0x56, 0x59, 0x37, 0xa0, 0x30, 0x39, 0xdd,
      0xd1, 0x5f, 0x66, 0x2e, 0x3c, 0xea, 0x58, 0x22, 0x6a, 0xd9, 0xcb, 0xdc,
      0x55, 0x93, 0x47, 0x4a, 0x6b, 0xbd, 0x96, 0xac, 0x2d, 0x57, 0x8e, 0x41,
      0x46, 0x4b, 0xae, 0x5b, 0xd7, 0xcc, 0x82, 0xad, 0xdb, 0x54, 0x33, 0xa6,
      0x80, 0xd0, 0x40, 0x2a, 0x36, 0x6c, 0x25, 0xf4, 0x8a, 0x52, 0x51, 0x85,
      0x89, 0x3d, 0x61, 0x29, 0xc6, 0xe2, 0x8d, 0x91, 0x72, 0x3b, 0x69, 0x88,
      0x7e, 0xe1, 0x42, 0x6e, 0x32, 0xe6, 0x7c, 0x92, 0x98, 0xba, 0x73, 0x5d,
      0xd6, 0xe5, 0x24, 0xe7, 0xbb, 0x20, 0xb9, 0x7a, 0x3f, 0xce, 0x8f, 0xc8,
      0xc0, 0x6f, 0xf6, 0xf5, 0xc2, 0xcf, 0x8b, 0xb5, 0xa9, 0xa3, 0xe8, 0xf9,
      0x65, 0xd8, 0x26, 0xa5,
    },
    { 0x00, 0x0d, 0x16, 0x13, 0x09, 0x0c, 0x10, 0x03, 0x04, 0x12, 0x17, 0x07,
      0x0e, 0x05, 0x01, 0x06, 0x0b, 0x08, 0x0a, 0x18, 0x1b, 0x1a, 0x1c, 0x15,
      0x1d, 0x02, 0x19, 0x14, 0x1f, 0x1e, 0x11, 0x0f, 0x3f, 0xa0, 0xfd, 0x62,
      0xf5, 0x9b, 0x38, 0xfe, 0xd3, 0x40, 0x28, 0xf8, 0xe8, 0x21, 0xbc, 0xe6,
      0x5d, 0x5e, 0xac, 0x71, 0x35, 0x91, 0x2e, 0xfc, 0xec, 0xef, 0x78, 0x9c,
      0xba, 0xed, 0xa6, 0xae, 0xfb, 0x3e, 0xf7, 0xf1, 0xb8, 0x65, 0x53, 0x44,
      0x75, 0xd5, 0xc4, 0xbf, 0x4a, 0x73, 0xe5, 0xee, 0x5b, 0x58, 0x49, 0x2d,
      0x79, 0x69, 0xe7, 0x51, 0x4f, 0x89, 0xc6, 0xd6, 0x61, 0x8e, 0x3a, 0x99,
      0x7e, 0x57, 0x9e, 0x24, 0xde, 0xa4, 0x7f, 0x70, 0x5c, 0x32, 0xc3, 0x88,
      0xf4, 0x23, 0xa2, 0x5f, 0xd9, 0x76, 0xd4, 0x6a, 0xc0, 0x85, 0xbd, 0xaf,
      0x64, 0x54, 0x95, 0x52, 0xff, 0x6b, 0xea, 0xf2, 0x33, 0xe1, 0xc2, 0xa3,
      0x2c, 0xb2, 0x4d, 0x7d, 0x6d, 0xd8, 0xb6, 0x9f, 0x2a, 0xad, 0xe2, 0x2b,
      0x56, 0xce, 0xdc, 0x96, 0x29, 0x77, 0x68, 0x6e, 0x43, 0xf6, 0xb4, 0xa9,
      0xf9, 0xc1, 0xa7, 0x41, 0x4e, 0x6c, 0x81, 0x2f, 0x39, 0xdd, 0xb1, 0x87,
      0x22, 0x4b, 0x80, 0xeb, 0x83, 0x6f, 0x8f, 0xb0, 0xb7, 0xe9, 0x46, 0x42,
      0x98, 0x5a, 0x45, 0x7c, 0xaa, 0x30, 0x74, 0xb3, 0x3d, 0xda, 0xe4, 0xf0,
      0x82, 0xb9, 0x9d, 0x66, 0xe0, 0x90, 0xd7, 0xcc, 0x37, 0x25, 0x84, 0xd2,
      0xa8, 0x8a, 0x50, 0x8b, 0x48, 0x7b, 0x63, 0xcb, 0x92, 0x34, 0xd1, 0xbe,
      0x60, 0x93, 0x36, 0x47, 0x59, 0xdf, 0xb5, 0xe3, 0xc9, 0xab, 0x8d, 0x3c,
      0xca, 0x9a, 0xa1, 0xfa, 0x86, 0x20, 0xc8, 0xc5, 0x27, 0x94, 0x72, 0xf3,
      0xd0, 0x8c, 0x4c, 0x7a, 0x55, 0xc7, 0x3b, 0xa5, 0xcd, 0x67, 0x97, 0x31,
      0x26, 0xcf, 0xbb, 0xdb,
    },
    { 0x00, 0x07, 0x0c, 0x03, 0x0f, 0x06, 0x11, 0x01, 0x05, 0x14, 0x16, 0x12,
      0x13, 0x04, 0x0e, 0x10, 0x09, 0x0a, 0x02, 0x17, 0x0d, 0x0b, 0x08, 0x1b,
      0x15, 0x1e, 0x1f, 0x1c, 0x19, 0x1d, 0x18, 0x1a, 0x35, 0xc1, 0xfd, 0xec,
      0xcb, 0xee, 0xde, 0xfe, 0xce, 0xd0, 0xb2, 0x2b, 0x65, 0xd2, 0xa7, 0x80,
      0x59, 0xeb, 0x3b, 0xa5, 0xd7, 0xed, 0xcd, 0x57, 0xbc, 0x49, 0x6b, 0x32,
      0x69, 0xbe, 0xf0, 0xa9, 0x63, 0xbd, 0x37, 0x89, 0xc9, 0x50, 0xea, 0x76,
      0x60, 0x5b, 0xf2, 0xd9, 0xc2, 0x31, 0x42, 0xca, 0xb5, 0x61, 0xe9, 0x47,
      0xc3, 0xae, 0xf8, 0x70, 0x3e, 0x23, 0xbf, 0xf5, 0x7a, 0x24, 0xd6, 0x56,
      0x30, 0x8b, 0xc7, 0x93, 0x68, 0xc8, 0x91, 0xc6, 0xc4, 0x8a, 0x2e, 0x96,
      0xd5, 0x55, 0x4b, 0x79, 0x51, 0x38, 0x9a, 0xc5, 0x54, 0x71, 0x6d, 0xe1,
      0xcf, 0x25, 0xdf, 0x3f, 0xff, 0x4f, 0xfa, 0xfc, 0x48, 0x7e, 0xf1, 0xef,
      0xe5, 0x97, 0x8d, 0xb0, 0x52, 0x28, 0x99, 0x39, 0x2c, 0x3a, 0xb9, 0xe4,
      0x5d, 0x46, 0x4d, 0xaa, 0xe3, 0x8c, 0xb8, 0x75, 0xdd, 0x8e, 0x9c, 0x9e,
      0x66, 0xa4, 0x36, 0xb6, 0x9d, 0xb1, 0x92, 0x40, 0x6f, 0x27, 0x53, 0x98,
      0xe0, 0xad, 0x3c, 0x2d, 0x22, 0x33, 0x7d, 0x20, 0xbb, 0xf9, 0x72, 0x77,
      0x95, 0x2f, 0xaf, 0x4e, 0xb3, 0x7c, 0x78, 0xcc, 0xe7, 0x58, 0x73, 0xfb,
      0xd1, 0x5f, 0xa0, 0x87, 0x2a, 0x41, 0xf6, 0x7b, 0x3d, 0x5a, 0x6c, 0x29,
      0x88, 0xba, 0x44, 0xa3, 0xe8, 0xdb, 0x84, 0x6e, 0x34, 0xe6, 0x64, 0x8f,
      0xa1, 0x9b, 0xa6, 0xdc, 0x90, 0xd3, 0xd4, 0xf7, 0x7f, 0x85, 0xa8, 0x5e,
      0x5c, 0x62, 0x9f, 0x94, 0xb4, 0x21, 0x45, 0xe2, 0x81, 0xab, 0x26, 0xac,
      0xf4, 0x4a, 0x74, 0xb7, 0xd8, 0x6a, 0x86, 0xa2, 0xf3, 0x43, 0x67, 0x4c,
      0xda, 0x83, 0x82, 0xc0,
    },
    { 0x00, 0x04, 0x11, 0x02, 0x12, 0x0d, 0x0c, 0x18, 0x0a, 0x03, 0x01, 0x05,
      0x10, 0x06, 0x14, 0x07, 0x0f, 0x15, 0x0b, 0x09, 0x1a, 0x16, 0x1e, 0x1f,
      0x1d, 0x1c, 0x08, 0x1b, 0x17, 0x13, 0x19, 0x0e, 0xca, 0xf8, 0xfd, 0xb2,
      0xf9, 0x89, 0xe5, 0xfe, 0x70, 0x73, 0xc7, 0x64, 0x53, 0x9f, 0x47, 0xe4,
      0xdd, 0xe9, 0xa8, 0x39, 0x3f, 0xa4, 0xc2, 0x2d, 0xdb, 0xb1, 0xe2, 0xe0,
      0x42, 0xc0, 0xc1, 0xa3, 0xd9, 0xe3, 0xda, 0x4c, 0x94, 0x67, 0x68, 0x2e,
      0xc4, 0x90, 0x87, 0xa6, 0xeb, 0x31, 0x61, 0x2b, 0x6d, 0x72, 0x9b, 0x32,
      0x69, 0x36, 0xa1, 0x52, 0x82, 0xd5, 0xd7, 0xfb, 0x4a, 0xbf, 0xfa, 0xe1,
      0x22, 0x91, 0x3d, 0xf6, 0xf2, 0xb9, 0xf5, 0x40, 0x54, 0x5c, 0xde, 0xd8,
      0x8f, 0xa7, 0xc6, 0x50, 0x78, 0x46, 0x3a, 0xcd, 0xd1, 0x76, 0x56, 0x6b,
      0xa9, 0xaa, 0xd3, 0xae, 0xff, 0xac, 0xfc, 0x4f, 0x3c, 0xd2, 0x8e, 0x5e,
      0x7a, 0x60, 0xf4, 0x7c, 0x9c, 0xc8, 0x74, 0xce, 0xc9, 0x83, 0x49, 0x55,
      0xad, 0xf1, 0xcf, 0xd4, 0x57, 0x2f, 0x30, 0xbc, 0x81, 0x88, 0x44, 0x63,
      0xb0, 0x5b, 0xed, 0x23, 0x79, 0x26, 0x4e, 0x8b, 0x7f, 0xc5, 0x48, 0x98,
      0x29, 0xb7, 0xee, 0x62, 0x25, 0xab, 0x66, 0xcc, 0xdf, 0x28, 0x7d, 0xdc,
      0x37, 0x7b, 0x3e, 0xe7, 0xc3, 0x43, 0xbd, 0xe8, 0xb8, 0x2a, 0x35, 0xa5,
      0x92, 0x6a, 0x8d, 0x41, 0x34, 0x9e, 0x4b, 0x65, 0xef, 0x96, 0x77, 0x84,
      0x2c, 0xe6, 0x71, 0x5a, 0xb4, 0x58, 0x27, 0xf3, 0xd0, 0x51, 0x93, 0x75,
      0x5d, 0x9a, 0x97, 0x6f, 0x8a, 0x20, 0x3b, 0x9d, 0x33, 0x38, 0x45, 0xa0,
      0x85, 0xbb, 0xbe, 0x86, 0xf7, 0x21, 0xaf, 0xea, 0x80, 0x5f, 0xec, 0x8c,
      0x6e, 0xb6, 0xa2, 0x95, 0xb3, 0x24, 0xcb, 0x4d, 0x99, 0xf0, 0xba, 0xb5,
      0x59, 0xd6, 0x6c, 0x7e,
    },
    { 0x00, 0x12, 0x09, 0x06, 0x08, 0x07, 0x0d, 0x16, 0x02, 0x0a, 0x04, 0x03,
      0x11, 0x01, 0x0e, 0x10, 0x18, 0x05, 0x0c, 0x14, 0x1d, 0x1b, 0x1f, 0x17,
      0x0b, 0x19, 0x0f, 0x1e, 0x13, 0x15, 0x1c, 0x1a, 0x6e, 0x32, 0xfd, 0xf7,
      0xd1, 0x2c, 0x96, 0xfe, 0xe7, 0xea, 0x81, 0x30, 0x2b, 0xb0, 0xf1, 0xaf,
      0x83, 0xec, 0xbf, 0xcf, 0x77, 0x4c, 0x98, 0xef, 0xfa, 0x89, 0xfb, 0x4f,
      0xbe, 0x37, 0xf9, 0xa1, 0xed, 0x49, 0x66, 0xac, 0x93, 0x43, 0xdf, 0xd7,
      0x64, 0x60, 0xb1, 0xf6, 0x95, 0x9f, 0x7b, 0x9c, 0x6f, 0x50, 0x94, 0x48,
      0x42, 0x5e, 0x31, 0x3f, 0x82, 0x86, 0x71, 0x24, 0x28, 0x68, 0x91, 0xe9,
      0x36, 0x22, 0xa8, 0xc6, 0x78, 0xd9, 0x97, 0xee, 0xa9, 0xaa, 0xba, 0x67,
      0x8b, 0xe5, 0x79, 0x5f, 0x9a, 0xf0, 0x47, 0x33, 0xb4, 0xeb, 0x59, 0x3b,
      0xb6, 0x2a, 0x92, 0x9d, 0xff, 0xbc, 0xd6, 0xdc, 0x25, 0x75, 0x7c, 0xa5,
      0x2f, 0x6c, 0x70, 0xde, 0xc8, 0x63, 0xb7, 0x57, 0x73, 0xbd, 0xf2, 0xca,
      0x45, 0x27, 0xad, 0x7e, 0xc9, 0xdd, 0xa7, 0x34, 0x7a, 0xd8, 0x54, 0xd4,
      0xc5, 0xb3, 0x8c, 0x3a, 0x8a, 0xe8, 0xc7, 0x74, 0xd5, 0xce, 0xb8, 0xd3,
      0x87, 0x65, 0x3e, 0x53, 0x20, 0xa6, 0x6b, 0x8d, 0xe6, 0x5a, 0x5c, 0x6a,
      0xc3, 0x9b, 0x90, 0x72, 0xcc, 0xb9, 0x46, 0x8f, 0xd2, 0x3c, 0x35, 0x80,
      0x44, 0xc4, 0x8e, 0x4a, 0x76, 0x23, 0x4b, 0xf3, 0x56, 0x52, 0xc0, 0x4e,
      0x55, 0xfc, 0xd0, 0xa3, 0xb2, 0xdb, 0x5b, 0x61, 0x2d, 0x88, 0x9e, 0x51,
      0x40, 0xda, 0x41, 0x26, 0xbb, 0xe2, 0x3d, 0x62, 0x6d, 0xa2, 0x2e, 0xe1,
      0xcb, 0xe0, 0x85, 0xe3, 0xa4, 0x21, 0xa0, 0x38, 0x39, 0xc1, 0x5d, 0xb5,
      0xab, 0x29, 0xf5, 0xf4, 0xae, 0xc2, 0x58, 0x99, 0x84, 0x7d, 0xe4, 0xf8,
      0x69, 0xcd, 0x4d, 0x7f,
    },
    { 0x00, 0x01, 0x10, 0x0d, 0x0b, 0x1a, 0x07, 0x0c, 0x0a, 0x09, 0x14, 0x13,
      0x02, 0x15, 0x12, 0x11, 0x06, 0x03, 0x05, 0x16, 0x19, 0x18, 0x1b, 0x0f,
      0x1c, 0x1f, 0x17, 0x0e, 0x1e, 0x1d, 0x08, 0x04, 0x82, 0x36, 0xfd, 0x41,
      0xf5, 0x28, 0xb9, 0xfe, 0xa9, 0xda, 0xc3, 0xf8, 0xdc, 0x93, 0x70, 0xd2,
      0x79, 0xdd, 0x4b, 0x78, 0x81, 0xb3, 0xd3, 0xfc, 0xe7, 0xec, 0x3f, 0x2a,
      0x6d, 0xe9, 0x3e, 0x4e, 0xfb, 0x4d, 0xf7, 0xf0, 0x64, 0x4f, 0xde, 0xcc,
      0xeb, 0xab, 0x7d, 0x74, 0x5a, 0x5d, 0xd1, 0xea, 0x9c, 0xae, 0xbb, 0x3c,
      0x54, 0x67, 0xd4, 0x69, 0x94, 0xc2, 0x83, 0xac, 0x89, 0x46, 0xd7, 0x24,
      0x57, 0xad, 0x32, 0x48, 0xc6, 0x3b, 0xc1, 0x53, 0xf3, 0x59, 0x7a, 0xa5,
      0xf4, 0x6b, 0x38, 0xa4, 0xb7, 0xa0, 0xaa, 0xd5, 0x75, 0xbd, 0x71, 0x50,
      0xbe, 0x88, 0x2c, 0x7b, 0xff, 0xe2, 0xe1, 0xf1, 0x6f, 0xcd, 0x77, 0x3a,
      0xe6, 0x56, 0x8a, 0x66, 0x9e, 0xb4, 0x62, 0x33, 0x8f, 0x4c, 0xce, 0x30,
      0x2d, 0x98, 0xc4, 0x8e, 0x68, 0xa6, 0xed, 0x20, 0x92, 0xf6, 0x60, 0x43,
      0xf9, 0x76, 0x40, 0xa7, 0xe4, 0x5b, 0xa8, 0x35, 0xb5, 0xc5, 0x52, 0x5c,
      0x5e, 0x86, 0x34, 0xe5, 0xa3, 0x8d, 0xcb, 0x51, 0x63, 0xdf, 0xb0, 0x9d,
      0x22, 0x80, 0x39, 0x7f, 0x47, 0x23, 0xd6, 0x58, 0xb2, 0xbf, 0xd0, 0xee,
      0xd8, 0x6a, 0x2f, 0xb8, 0xca, 0xba, 0xb1, 0x91, 0xbc, 0x72, 0x26, 0xa2,
      0x42, 0xe3, 0xef, 0x65, 0xc8, 0x4a, 0xb6, 0x90, 0xaf, 0x6c, 0xa1, 0x73,
      0x55, 0x5f, 0xd9, 0x97, 0x9a, 0xc9, 0x61, 0xcf, 0x8b, 0x49, 0xc7, 0x87,
      0x8c, 0x25, 0x37, 0xfa, 0x31, 0x21, 0x85, 0x7e, 0xe0, 0x27, 0x99, 0xf2,
      0x9f, 0xdb, 0x44, 0x7c, 0xe8, 0x84, 0x45, 0x3d, 0x96, 0x2b, 0x29, 0x95,
      0x2e, 0x9b, 0x6e, 0xc0,
    },
    { 0x00, 0x07, 0x0c, 0x03, 0x0f, 0x06, 0x11, 0x01, 0x05, 0x14, 0x16, 0x12,
      0x13, 0x04, 0x0e, 0x10, 0x09, 0x0a, 0x02, 0x17, 0x0d, 0x0b, 0x08, 0x1b,
      0x15, 0x1e, 0x1f, 0x1c, 0x19, 0x1d, 0x18, 0x1a, 0x35, 0xc1, 0xfd, 0xec,
      0xcb, 0xee, 0xde, 0xfe, 0xce, 0xd0, 0xb2, 0x2b, 0x65, 0xd2, 0xa7, 0x80,
      0x59, 0xeb, 0x3b, 0xa5, 0xd7, 0xed, 0xcd, 0x57, 0xbc, 0x49, 0x6b, 0x32,
      0x69, 0xbe, 0xf0, 0xa9, 0x63, 0xbd, 0x37, 0x89, 0xc9, 0x50, 0xea, 0x76,
      0x60, 0x5b, 0xf2, 0xd9, 0xc2, 0x31, 0x42, 0xca, 0xb5, 0x61, 0xe9, 0x47,
      0xc3, 0xae, 0xf8, 0x70, 0x3e, 0x23, 0xbf, 0xf5, 0x7a, 0x24, 0xd6, 0x56,
      0x30, 0x8b, 0xc7, 0x93, 0x68, 0xc8, 0x91, 0xc6, 0xc4, 0x8a, 0x2e, 0x96,
      0xd5, 0x55, 0x4b, 0x79, 0x51, 0x38, 0x9a, 0xc5, 0x54, 0x71, 0x6d, 0xe1,
      0xcf, 0x25, 0xdf, 0x3f, 0xff, 0x4f, 0xfa, 0xfc, 0x48, 0x7e, 0xf1, 0xef,
      0xe5, 0x97, 0x8d, 0xb0, 0x52, 0x28, 0x99, 0x39, 0x2c, 0x3a, 0xb9, 0xe4,
      0x5d, 0x46, 0x4d, 0xaa, 0xe3, 0x8c, 0xb8, 0x75, 0xdd, 0x8e, 0x9c, 0x9e,
      0x66, 0xa4, 0x36, 0xb6, 0x9d, 0xb1, 0x92, 0x40, 0x6f, 0x27, 0x53, 0x98,
      0xe0, 0xad, 0x3c, 0x2d, 0x22, 0x33, 0x7d, 0x20, 0xbb, 0xf9, 0x72, 0x77,
      0x95, 0x2f, 0xaf, 0x4e, 0xb3, 0x7c, 0x78, 0xcc, 0xe7, 0x58, 0x73, 0xfb,
      0xd1, 0x5f, 0xa0, 0x87, 0x2a, 0x41, 0xf6, 0x7b, 0x3d, 0x5a, 0x6c, 0x29,
      0x88, 0xba, 0x44, 0xa3, 0xe8, 0xdb, 0x84, 0x6e, 0x34, 0xe6, 0x64, 0x8f,
      0xa1, 0x9b, 0xa6, 0xdc, 0x90, 0xd3, 0xd4, 0xf7, 0x7f, 0x85, 0xa8, 0x5e,
      0x5c, 0x62, 0x9f, 0x94, 0xb4, 0x21, 0x45, 0xe2, 0x81, 0xab, 0x26, 0xac,
      0xf4, 0x4a, 0x74, 0xb7, 0xd8, 0x6a, 0x86, 0xa2, 0xf3, 0x43, 0x67, 0x4c,
      0xda, 0x83, 0x82, 0xc0,
    },
    { 0x00, 0x04, 0x11, 0x02, 0x12, 0x0d, 0x0c, 0x18, 0x0a, 0x03, 0x01, 0x05,
      0x10, 0x06, 0x14, 0x07, 0x0f, 0x15, 0x0b, 0x09, 0x1a, 0x16, 0x1e, 0x1f,
      0x1d, 0x1c, 0x08, 0x1b, 0x17, 0x13, 0x19, 0x0e, 0xca, 0xf8, 0xfd, 0xb2,
      0xf9, 0x89, 0xe5, 0xfe, 0x70, 0x73, 0xc7, 0x64, 0x53, 0x9f, 0x47, 0xe4,
      0xdd, 0xe9, 0xa8, 0x39, 0x3f, 0xa4, 0xc2, 0x2d, 0xdb, 0xb1, 0xe2, 0xe0,
      0x42, 0xc0, 0xc1, 0xa3, 0xd9, 0xe3, 0xda, 0x4c, 0x94, 0x67, 0x68, 0x2e,
      0xc4, 0x90, 0x87, 0xa6, 0xeb, 0x31, 0x61, 0x2b, 0x6d, 0x72, 0x9b, 0x32,
      0x69, 0x36, 0xa1, 0x52, 0x82, 0xd5, 0xd7, 0xfb, 0x4a, 0xbf, 0xfa, 0xe1,
      0x22, 0x91, 0x3d, 0xf6, 0xf2, 0xb9, 0xf5, 0x40, 0x54, 0x5c, 0xde, 0xd8,
      0x8f, 0xa7, 0xc6, 0x50, 0x78, 0x46, 0x3a, 0xcd, 0xd1, 0x76, 0x56, 0x6b,
      0xa9, 0xaa, 0xd3, 0xae, 0xff, 0xac, 0xfc, 0x4f, 0x3c, 0xd2, 0x8e, 0x5e,
      0x7a, 0x60, 0xf4, 0x7c, 0x9c, 0xc8, 0x74, 0xce, 0xc9, 0x83, 0x49, 0x55,
      0xad, 0xf1, 0xcf, 0xd4, 0x57, 0x2f, 0x30, 0xbc, 0x81, 0x88, 0x44, 0x63,
      0xb0, 0x5b, 0xed, 0x23, 0x79, 0x26, 0x4e, 0x8b, 0x7f, 0xc5, 0x48, 0x98,
      0x29, 0xb7, 0xee, 0x62, 0x25, 0xab, 0x66, 0xcc, 0xdf, 0x28, 0x7d, 0xdc,
      0x37, 0x7b, 0x3e, 0xe7, 0xc3, 0x43, 0xbd, 0xe8, 0xb8, 0x2a, 0x35, 0xa5,
      0x92, 0x6a, 0x8d, 0x41, 0x34, 0x9e, 0x4b, 0x65, 0xef, 0x96, 0x77, 0x84,
      0x2c, 0xe6, 0x71, 0x5a, 0xb4, 0x58, 0x27, 0xf3, 0xd0, 0x51, 0x93, 0x75,
      0x5d, 0x9a, 0x97, 0x6f, 0x8a, 0x20, 0x3b, 0x9d, 0x33, 0x38, 0x45, 0xa0,
      0x85, 0xbb, 0xbe, 0x86, 0xf7, 0x21, 0xaf, 0xea, 0x80, 0x5f, 0xec, 0x8c,
      0x6e, 0xb6, 0xa2, 0x95, 0xb3, 0x24, 0xcb, 0x4d, 0x99, 0xf0, 0xba, 0xb5,
      0x59, 0xd6, 0x6c, 0x7e,
    },
};

// Decrypt the buffer ``buf`` in-place.
// ``offset`` is the position of ``buf`` within the file being read
// (measured as bytes from the beginning of the file).
static void decrypt_buf(std::string &buf, unsigned long long int salt) {
    // Scribble directly into buf using pointer arithmetic.
    // Using iterators would be better, but it's too slow in debug builds.
    unsigned char salt_char = static_cast<unsigned char>(salt);
    unsigned char *data = (unsigned char*)buf.data();
    size_t buf_size = buf.size();
    for (unsigned int i = 0; i < buf_size; i++) {
        unsigned char c = data[i];
        data[i] = cypher_table[(salt_char++) & 7][c];
    }
}


unsigned int GVGHeader::count_gauges() {
    if (!Gauges.size())
        return 0;
    return 1 + std::count(Gauges.cbegin(), Gauges.cend(), L' ');
}

static bool stobool(const std::wstring &s) {
    if (s == L"yes")
        return true;
    if (s == L"no")
        return false;
    throw std::runtime_error("Could not convert string to bool.");
}

static unsigned long int hextoul(const std::wstring &s) {
    return std::stoul(s, 0, 16);
}

// Pass by value because we need to modify s.
static std::wstring unescape_str(std::wstring s) {
    replace_all(s, L"\\r\\n", L"\n");
    replace_all(s, L"\\n", L"\n");
    replace_all(s, L"\\t", L"\t");
    return s;
}

#define try_set_field(field, expr) \
    if (key == L#field) {          \
        field = (expr);            \
        return true;               \
    }

bool GVGHeader::set_field(const std::wstring &key, const std::wstring &value) {
    try_set_field(FileVersion, std::stof(value));
    try_set_field(VendorCode, hextoul(value));
    try_set_field(ProductCode, hextoul(value));
    try_set_field(CopyrightInfo, unescape_str(value));
    try_set_field(LicenseInfo, unescape_str(value));
    try_set_field(MapInfo, unescape_str(value));
    try_set_field(Title, unescape_str(value));
    try_set_field(Description, unescape_str(value));
    try_set_field(AutoLayer, value);
    try_set_field(HideLayer, value);
    try_set_field(AutoSwitch, stobool(value));
    try_set_field(AutoTile, stobool(value));
    try_set_field(ObjectScale, std::stof(value));
    try_set_field(CacheMode, stobool(value));
    try_set_field(AutoFrame, stobool(value));
    try_set_field(BkColor, hextoul(value));
    try_set_field(StretchNice, stobool(value));
    try_set_field(Gauges, value);
    return false;
}

bool GVGMapInfo::set_field(const std::wstring &key,
                           const std::wstring &value)
{
    try_set_field(Type, value);
    try_set_field(Path, value);
    try_set_field(Brightness, value);
    try_set_field(Scale, std::stoi(value));
    try_set_field(Ellipsoid, value);
    try_set_field(Projection, value);
    try_set_field(BaseMed, std::stod(value));
    try_set_field(Zone, std::stoi(value));
    try_set_field(OffsetEast, std::stod(value));
    try_set_field(OffsetNorth, std::stod(value));
    try_set_field(WorldOrgX, std::stod(value));
    try_set_field(WorldOrgY, std::stod(value));
    try_set_field(WPPX, std::stod(value));
    try_set_field(WPPY, std::stod(value));
    try_set_field(RADX, std::stod(value));
    try_set_field(RADY, std::stod(value));
    try_set_field(ImageWidth, std::stoi(value));
    try_set_field(ImageHeight, std::stoi(value));
    try_set_field(BorderPly, value);
    try_set_field(LegendImage, value);
    return false;
}
#undef try_set_field

void GVGMapInfo::Pixel_to_PCS(double x_px, double y_px,
                              double* x_pcs, double* y_pcs) const
{
    *x_pcs = (RADX_cos * x_px - RADX_sin * y_px) * WPPX + WorldOrgX;
    *y_pcs = (RADY_sin * x_px + RADY_cos * y_px) * WPPY + WorldOrgY;
}

void GVGMapInfo::PCS_to_Pixel(double x_pcs, double y_pcs,
                              double* x_px, double* y_px) const
{
    double x, y;
    x = (x_pcs - WorldOrgX) / WPPX;
    y = (y_pcs - WorldOrgY) / WPPY;

    // Typically, RADX will be equal to RADY -> denominator == 1, but be
    // prepared for weird cases.
    double denominator = RADX_cos * RADY_cos + RADX_sin * RADY_sin;
    *x_px = (+RADY_cos*x + RADX_sin*y) / denominator;
    *y_px = (-RADY_sin*x + RADX_cos*y) / denominator;
}

void GVGMapInfo::complete_initialization() {
    if (OffsetEast == 0.0) {
        // Match the behavior of the reference implementation.
        OffsetEast = DEFAULT_OFFSET_EAST;
        WorldOrgX += DEFAULT_OFFSET_EAST;
    }
    RADX_sin = sin(RADX * DEG_to_RAD);
    RADX_cos = cos(RADX * DEG_to_RAD);
    RADY_sin = sin(RADY * DEG_to_RAD);
    RADY_cos = cos(RADY * DEG_to_RAD);
}


GVGFile::GVGFile(const std::wstring &fname)
: m_fname(fname), m_header(), m_mapinfos(), m_decoded_data()
{
    std::ifstream file(m_fname, std::ios::in | std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();
    if (!data.size())
        throw std::runtime_error("Failed to read GVG metadata.");
    decrypt_buf(data, 0);
    m_decoded_data = WStringFromString(data, GVG_ENCODING);
    ParseINI(m_decoded_data);
}

void GVGFile::ParseINI(const std::wstring &data) {
    std::wregex re_header(L"\\[Header\\]");
    std::wregex re_map(L"\\[MAP\\d*\\]");
    std::wregex re_keyvalue(L"(.*)=(.*)");

    enum parse_mode { PM_NONE, PM_HEADER, PM_MAPINFO };
    enum parse_mode pm = PM_NONE;
    std::wstringstream buf(data);
    std::wstring line;
    while(std::getline(buf, line)) {
        if (line.back() == '\r')
            line.pop_back();
        if (!line.size())
            continue;

        std::wsmatch match;
        if (std::regex_match(line, re_header)) {
            if (pm != PM_NONE)
                // Allow only one header block, right at the beginning.
                throw std::runtime_error("Multiple [Header] entries in GVG.");
            pm = PM_HEADER;
        } else if (std::regex_match(line, re_map)) {
            if (pm == PM_NONE)
                throw std::runtime_error("No [Header] entry in GVG file.");
            pm = PM_MAPINFO;
            m_mapinfos.push_back(GVGMapInfo());
        } else if (std::regex_match(line, match, re_keyvalue)) {
            auto key = match[1].str();
            auto val = match[2].str();
            // Store the key/value in the current header/mapinfo.
            if (pm == PM_HEADER) {
                if (!m_header.set_field(key, val))
                    throw std::runtime_error("Unrecognized header field.");
            } else if (pm == PM_MAPINFO) {
                if (!m_mapinfos.back().set_field(key, val))
                    throw std::runtime_error("Unrecognized mapinfo field.");
            } else {
                throw std::runtime_error("Assignment before header stance.");
            }
        } else {
            throw std::runtime_error("Could not parse GVG metadata line.");
        }
    }
    if (m_mapinfos.size() != m_header.count_gauges())
        throw std::runtime_error("Mismatch: Gauges / [MAP] entries.");
    for (auto it=m_mapinfos.begin(); it != m_mapinfos.end(); ++it) {
        it->complete_initialization();
    }
}

unsigned int GVGFile::BestResolutionIndex() const {
    auto hi = m_mapinfos.cbegin();
    for (auto it=m_mapinfos.cbegin(); it != m_mapinfos.cend(); ++it) {
        if (it->ImageWidth > hi->ImageWidth)
            hi = it;
    }
    return hi - m_mapinfos.cbegin();
}

std::wstring GVGFile::ResolvePath(const std::wstring &gmp_name) const {
    size_t last_slash = m_fname.rfind(ODM_PathSep_wchar);
    if (last_slash == std::string::npos) {
        // No directory separator in GVG path: It is in the current directory.
        // Just return the GMP filename.
        return gmp_name;
    }
    return m_fname.substr(0, last_slash + 1) + gmp_name;
}

const GVGMapInfo &GVGFile::MapInfo(unsigned int n) const {
    return m_mapinfos[n];
}

std::tuple<std::wstring, unsigned int> GVGFile::GmpPath(unsigned int n) const {
    std::wstring fname = m_mapinfos[n].Path;
    size_t delim = fname.rfind('|');
    if (delim == std::string::npos) {
        return std::make_tuple(std::wstring(), 0);
    }
    std::wstring image_num_str = fname.substr(delim + 1);
    fname = fname.substr(0, delim);

    unsigned long int image_num;
    try {
        image_num = std::stoul(image_num_str);
    }
    catch(...) { return std::make_tuple(std::wstring(), 0); }

    return std::make_tuple(ResolvePath(fname), image_num);
}


GMPImage::GMPImage(const std::wstring &fname, int index, int64_t foffset) :
    m_file(fname, std::ios::in | std::ios::binary),
    m_fname(fname), m_findex(index), m_foffset(foffset),
    m_bfh(), m_bih_buf(), m_bih(nullptr), m_gmphdr(nullptr),
    m_tiles_x(0), m_tiles_y(0), m_tiles(0), m_tile_index(),
    m_topdown(false)
{
    Init();
}

GMPImage::GMPImage(const GMPImage &other) :
    // Create a new ifstream from the same filename.
    m_file(other.m_fname, std::ios::in | std::ios::binary),
    m_fname(other.m_fname),
    m_findex(other.m_findex), m_foffset(other.m_foffset),
    m_bfh(other.m_bfh),
    // Copy over m_bih_buf, make m_bih and m_gmphdr point to our own copy.
    // No sanity checks necessary, as we throw in Init() if anything is fishy.
    m_bih_buf(other.m_bih_buf),
    m_bih(reinterpret_cast<const GMPBitmapInfoHdr*>(&m_bih_buf[0])),
    m_gmphdr(reinterpret_cast<const GMPHeader*>(&m_bih_buf[m_bih->biSize])),
    m_tiles_x(other.m_tiles_x), m_tiles_y(other.m_tiles_y),
    m_tiles(other.m_tiles), m_tile_index(other.m_tile_index),
    m_topdown(other.m_topdown)
{ }

void GMPImage::Init() {
    m_file.seekg(m_foffset);
    m_file.read((char*)&m_bfh, sizeof(GMPBitmapFileHdr));
    if (m_bfh.bfType != 0x5847) {  // "GX"
        throw std::runtime_error("Wrong signature: not a valid GMP image");
    }

    m_file.seekg(m_foffset + sizeof(GMPBitmapFileHdr));
    m_bih_buf.resize(m_bfh.bfOffBits - sizeof(GMPBitmapFileHdr));
    m_file.read(&m_bih_buf[0], m_bih_buf.size());

    if (m_bih_buf.size() < sizeof(GMPBitmapInfoHdr)) {
        throw std::runtime_error("Invalid GMP Bitmap header: BIH missing.");
    }
    m_bih = reinterpret_cast<const GMPBitmapInfoHdr*>(&m_bih_buf[0]);

    if (m_bih_buf.size() < m_bih->biSize + sizeof(GMPHeader)) {
        throw std::runtime_error("Invalid GMP Bitmap header: GMPH missing.");
    }
    m_gmphdr = reinterpret_cast<const GMPHeader*>(&m_bih_buf[m_bih->biSize]);

    // The remainder of the header is just padding (\0 bytes).
    // header_rest = m_bih_buf.get() + m_bih->biSize + sizeof(GMPHeader);
    // header_rest_len = m_bfh.bfOffBits - sizeof(GMPBitmapFileHdr) -
    //                   m_bih->biSize - sizeof(GMPHeader);

    if (m_bih->biPlanes != 1 || !IsSupportedBPP(m_bih->biBitCount) ||
        m_bih->biCompression != 0)
    {
        throw std::runtime_error("Format not supported");
    }

    m_topdown = (m_bih->biHeight < 0);
    m_tiles_x = (AnnouncedWidth() + TileWidth() - 1) / TileWidth();
    m_tiles_y = (AnnouncedHeight() + TileHeight() - 1) / TileHeight();
    m_tiles = m_tiles_x * m_tiles_y;

    m_file.seekg(m_foffset + m_bfh.bfOffBits);
    m_tile_index.resize(m_tiles);
    m_file.read((char*)&m_tile_index[0],
                sizeof(struct GMPTileOffset) * m_tiles);
}

std::wstring GMPImage::DebugData() const {
    std::wostringstream ss;
    ss << string_format(L"path: %s|%d\n", m_fname.c_str(), m_findex);
    ss << string_format(L"bfh:\n"
                        L"  WORD bfType = %c%c\n"
                        L"  DWORD bfOffBits = %d\n",
                        m_bfh.bfType & 0xFF, m_bfh.bfType >> 8,
                        m_bfh.bfOffBits);
    ss << string_format(L"bih:\n"
                        L"  Width = %d; Height = %d\n"
                        L"  Planes = %d; BitCount = %d\n"
                        L"  Compression = %d\n",
                        m_bih->biWidth, m_bih->biHeight,
                        m_bih->biPlanes, m_bih->biBitCount,
                        m_bih->biCompression);
    ss << string_format(L"gmphdr:\n"
                        L"  unkn1 = %d\n"
                        L"  unkn2 = %d\n"
                        L"  compression = %d\n"
                        L"  tile_px_x = %d; tile_px_y = %d\n"
                        L"  unkn4 = %d\n",
                        m_gmphdr->unkn1, m_gmphdr->unkn2,
                        m_gmphdr->compression, m_gmphdr->tile_px_x,
                        m_gmphdr->tile_px_y, m_gmphdr->unkn4);
    return ss.str();
}

int64_t GMPImage::NextImageOffset() const {
    if (m_tile_index.size() == 0) {
        throw std::runtime_error("No tiles found in image.");
    }
    auto last_tile = m_tile_index.cbegin();
    for (auto it = m_tile_index.cbegin(); it != m_tile_index.cend(); ++it) {
        last_tile = (it->offset > last_tile->offset) ? it : last_tile;
    }
    return m_foffset + m_bfh.bfOffBits +
           last_tile->offset + last_tile->length;
}

int GMPImage::AnnouncedWidth() const { return m_bih->biWidth; };
int GMPImage::AnnouncedHeight() const { return abs(m_bih->biHeight); };
int GMPImage::RealWidth() const { return TileWidth() * NumTilesX(); };
int GMPImage::RealHeight() const { return TileHeight() * NumTilesY(); };
int GMPImage::TileWidth() const { return m_gmphdr->tile_px_x; };
int GMPImage::TileHeight() const { return m_gmphdr->tile_px_y; };
int GMPImage::NumTilesX() const { return m_tiles_x; };
int GMPImage::NumTilesY() const { return m_tiles_y; };
int GMPImage::BitsPerPixel() const { return m_bih->biBitCount; };

std::string GMPImage::LoadCompressedTile(long tx, long ty) const {
    if (tx < 0 || ty < 0 || tx >= (int)m_tiles_x || ty >= (int)m_tiles_y) {
        return std::string();
    }
    unsigned int idx;
    if (m_topdown) {
        idx = tx + m_tiles_x * ty;
    } else {
        idx = tx + m_tiles_x * (m_tiles_y - ty - 1);
    }
    if (m_tile_index[idx].offset == -1 && m_tile_index[idx].length == 0)
        return std::string();
    if (m_tile_index[idx].length > std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("Malformed tile index.");
    }
    size_t tile_buffer_sz = static_cast<size_t>(m_tile_index[idx].length);
    m_file.seekg(m_foffset + m_bfh.bfOffBits + m_tile_index[idx].offset);

    std::string tile(tile_buffer_sz, '\0');
    m_file.read(&tile[0], tile_buffer_sz);
    decrypt_buf(tile, m_tile_index[idx].length);
    return tile;
}

PixelBuf GMPImage::LoadTile(long tx, long ty) const {
    if (BitsPerPixel() == 24) {
        std::string tile = LoadCompressedTile(tx, ty);
        if (!tile.size()) {
            // Most likely a request outside of the image area.
            // Return a correctly sized dummy image.
            return PixelBuf(TileWidth(), TileHeight());
        }
        // Decompress jpeg while swapping R and B channels. No idea why
        // the data is encoded this way in the first place.
        PixelBuf res = decompress_jpeg(tile, true);
        if (!res.GetData()) {
            throw std::runtime_error("Failed to decompress tile");
        }
        return res;
    } else {
        // TODO: Add support for more formats.
        throw std::runtime_error("Not implemented");
    }
    return PixelBuf();
}

GMPImage MakeGmpImage(const std::wstring& path, unsigned int gmp_image_idx) {
    int64_t foffset = 0;
    auto fsize = GetFilesize(path);
    int64_t filesize = GetFilesize(path);
    for (int i = 0; foffset < filesize; i++) {
        GMPImage image(path, i, foffset);
        if (i == gmp_image_idx) {
            return image;
        }
        foffset = image.NextImageOffset();
    }
    throw std::runtime_error("Could not find GMP image.");
}


GMPImage MakeBestResolutionGmpImage(const GVGFile &gvgfile) {
    unsigned int idx_best_res = gvgfile.BestResolutionIndex();

    std::wstring path;
    int image_num;
    std::tie(path, image_num) = gvgfile.GmpPath(idx_best_res);

    return MakeGmpImage(path, image_num);
}

GVGMap::GVGMap(const std::wstring &fname)
    : m_gvgfile(fname), m_image(MakeBestResolutionGmpImage(m_gvgfile)),
      m_gvgheader(&m_gvgfile.Header()),
      m_gvgmapinfo(&m_gvgfile.MapInfo(m_gvgfile.BestResolutionIndex())),
      m_tile_width(m_image.TileWidth()),
      m_tile_height(m_image.TileHeight()),
      m_tiles_x(m_image.NumTilesX()),
      m_tiles_y(m_image.NumTilesY()),
      m_width(m_image.RealWidth()),
      m_height(m_image.RealHeight()),
      m_bpp(m_image.BitsPerPixel()),
      m_proj_str(MakeProjString()),
      m_proj(m_proj_str)
{
    // TODO: Add title and description information.
    // gvgh->Title;
    // gvgh->Description
}

std::string GVGMap::MakeProjString() const {
    std::ostringstream ss;
    if (m_gvgmapinfo->Projection == L"utm") {
        ss << "+proj=utm +zone=" << m_gvgmapinfo->Zone << " ";
    } else if (m_gvgmapinfo->Projection == L"gk") {
        ss << "+proj=tmerc +k=1 +datum=potsdam ";
        ss << "+lat_0=0 +lon_0=" << m_gvgmapinfo->BaseMed << " ";
        ss << "+x_0=" << m_gvgmapinfo->OffsetEast << " ";
        ss << "+y_0=" << m_gvgmapinfo->OffsetNorth << " ";
    } else {
        throw std::runtime_error("Unknown projection.");
    }

    if (m_gvgmapinfo->Ellipsoid == L"wgs84") {
        ss << "+ellps=WGS84 ";
    } else if (m_gvgmapinfo->Ellipsoid == L"bessel") {
        ss << "+ellps=bessel ";
    } else {
        throw std::runtime_error("Unknown ellipsoid.");
    }
    ss << "+units=m ";
    return ss.str();
}

GVGMap::~GVGMap() {}

RasterMap::DrawableType GVGMap::GetType() const {
    return RasterMap::TYPE_MAP;
}
unsigned int GVGMap::GetWidth() const {
    return m_width;
}
unsigned int GVGMap::GetHeight() const {
    return m_height;
}
MapPixelDeltaInt GVGMap::GetSize() const {
    return MapPixelDeltaInt(m_width, m_height);
}

PixelBuf
GVGMap::GetRegion(const MapPixelCoordInt &pos,
                  const MapPixelDeltaInt &size) const
{
    auto fixed_bounds_pb = GetRegion_BoundsHelper(*this, pos, size);
    if (fixed_bounds_pb.GetData())
        return fixed_bounds_pb;

    PixelBuf result(size.x, size.y);
    MapPixelCoordInt end = pos + size;

    int first_ty = pos.y / m_tile_height;
    int last_ty = (end.y - 1) / m_tile_height;
    for (int ty = pos.y / m_tile_height; ty*m_tile_height < end.y; ty++) {
        for (int tx = pos.x / m_tile_width; tx*m_tile_width < end.x; tx++) {
            PixelBuf tile = m_image.LoadTile(tx, ty);
            if (!tile.GetData()) {
                continue;
            }
            auto insert_pos = PixelBufCoord(
                    tx * m_tile_width - pos.x,
                    (last_ty - ty + first_ty) * m_tile_height - pos.y);
            result.Insert(insert_pos, tile);
        }
    }
    return result;
}

Projection GVGMap::GetProj() const {
    return m_proj;
}

const std::wstring &GVGMap::GetFname() const {
    return m_gvgfile.Filename();
}

const std::wstring &GVGMap::GetTitle() const {
    return m_gvgfile.Header().Title;
}

const std::wstring &GVGMap::GetDescription() const {
    return m_gvgfile.Header().Description;
}

ODMPixelFormat GVGMap::GetPixelFormat() const {
    return ODM_PIX_RGBX4;
}

bool GVGMap::PixelToLatLon(const MapPixelCoord &pos, LatLon *result) const {
    double x = pos.x;
    double y = m_tiles_y * m_tile_height - pos.y - 1;
    double cx, cy;
    m_gvgmapinfo->Pixel_to_PCS(x, y, &cx, &cy);
    if (!GetProj().PCSToLatLong(cx, cy)) {
        return false;
    }
    *result = LatLon(cy, cx);
    return true;
}

bool GVGMap::LatLonToPixel(const LatLon &pos, MapPixelCoord *result) const {
    double x = pos.lon;
    double y = pos.lat;
    if (!GetProj().LatLongToPCS(x, y)) {
        return false;
    }
    m_gvgmapinfo->PCS_to_Pixel(x, y, &result->x, &result->y);
    result->y = m_tiles_y * m_tile_height - result->y - 1;
    return true;
}


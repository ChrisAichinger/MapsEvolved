// gvg2geotiff.cpp: Convert GVG maps to GeoTIFF

#include <array>
#include <string>
#include <iostream>

#include "../OutdoorMapper/map_gvg.h"
#include "../OutdoorMapper/util.h"

#include "tiff.h"
#include "geotiff.h"
#include "geovalues.h"
#include "libxtiff/xtiffio.h"

#include "getopt.h"

const char * const DEFAULT_ENCODING = "UTF-8";
const int DEFAULT_JPEG_COMPRESSION = 75;


typedef std::unique_ptr<TIFF, decltype(&XTIFFClose)> TIFF_unique_ptr;
auto GTIF_Deleter = [](GTIF* ptr) {
    GTIFWriteKeys(ptr);
    GTIFFree(ptr);
};
typedef std::unique_ptr<GTIF, decltype(GTIF_Deleter)> GTIF_unique_ptr;

struct compression_def {
    uint16 compression;
    int jpegcolormode;
    int jpegquality;
    uint16 predictor;
};


static void parse_compress_opts(wchar_t* opt, compression_def *comp);
static void usage(void);

using std::wcout;
using std::wcerr;
using std::endl;


void invert_y(PixelBuf &pb) {
    // In-place convert pb from top-down to bottom-up row order, or vice versa.
    auto buf = pb.GetRawData();
    auto width = pb.GetWidth();
    auto height = pb.GetHeight();
    for (unsigned int y = 0; y < height / 2; y++) {
        for (unsigned int x = 0; x < width; x++) {
            std::swap(*pb.GetPixelPtr(x, y),
                      *pb.GetPixelPtr(x, height - y - 1));
        }
    }
}

void rgb32_to_rgb24(PixelBuf &pb) {
    // In-place convert the standard 32 bit RGB pixelbuf (RGBX/RGBA) to a
    // packed RGB representation (24 bit RGB).
    auto uint_buf = pb.GetRawData();
    auto uchar_buf = reinterpret_cast<unsigned char*>(uint_buf);

    unsigned int num_pixels = pb.GetWidth() * pb.GetHeight();
    for (unsigned int i = 0; i < num_pixels; i++) {
        unsigned int val = uint_buf[i];
        *reinterpret_cast<unsigned int*>(&uchar_buf[3*i]) = val;
    }
}

void set_tiff_keys(GVGMap *map, TIFF *tif, const compression_def *comp) {
    uint16 nbands, depth;
    uint16 photometric, samplefmt;
    if (map->GetType() != GeoDrawable::TYPE_DHM) {
        // Rely on only getting RGB PixelBuf's.
        nbands = 3;
        depth = 8;
        photometric = PHOTOMETRIC_RGB;
        samplefmt = SAMPLEFORMAT_UINT;
    } else {
        nbands = 1;
        depth = 16;    // FIXME: Set correct bits / sample for DHMs.
        photometric = PHOTOMETRIC_MINISBLACK;
        samplefmt = SAMPLEFORMAT_INT;
    }

    float resolution = 0.0f;
    short int res_unit = RESUNIT_NONE;
    // GVG maps don't provide a scan resolution.
    // Try to guess the correct resolution from the Vendor/Product ID.
    if (map->GetGVGFile().Header().VendorCode == 0x0034 &&
        map->GetGVGFile().Header().ProductCode == 0x3afd)
    {
        resolution = 100.0;  // pixel/cm.
        res_unit = RESUNIT_CENTIMETER;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, map->GetWidth());
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, map->GetHeight());
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, nbands);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, depth);
    TIFFSetField(tif, TIFFTAG_TILEWIDTH, map->GetGMPImage().TileWidth());
    TIFFSetField(tif, TIFFTAG_TILELENGTH, map->GetGMPImage().TileHeight());
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, resolution);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, resolution);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, res_unit);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, samplefmt);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, comp->compression);
    switch (comp->compression) {
        case COMPRESSION_JPEG:
            if (photometric == PHOTOMETRIC_RGB &&
                comp->jpegcolormode == JPEGCOLORMODE_RGB)
            {
                photometric = PHOTOMETRIC_YCBCR;
            }
            TIFFSetField(tif, TIFFTAG_JPEGQUALITY, comp->jpegquality);
            TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, comp->jpegcolormode);
            break;
        case COMPRESSION_LZW:
        case COMPRESSION_DEFLATE:
            if (comp->predictor != 0)
                TIFFSetField(tif, TIFFTAG_PREDICTOR, comp->predictor);
            break;
    }

    auto title = StringFromWString(map->GetTitle(), DEFAULT_ENCODING);
    TIFFSetField(tif, TIFFTAG_DOCUMENTNAME, title);
    auto desc = StringFromWString(map->GetDescription(), DEFAULT_ENCODING);
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, desc);
}

void copy_tiles(GVGMap *map, TIFF *tif) {
    wcerr << L"Converting: ";
    auto num_tiles_x = map->GetGMPImage().NumTilesX();
    auto num_tiles_y = map->GetGMPImage().NumTilesY();
    for (int ty = 0; ty < num_tiles_y; ty++) {
        wcerr << L".";
        for (int tx = 0; tx < num_tiles_x; tx++) {
            auto pb = map->GetGMPImage().LoadTile(tx, ty);
            invert_y(pb);
            rgb32_to_rgb24(pb);
            int tile_size = pb.GetWidth() * pb.GetHeight() * 3;
            TIFFWriteEncodedTile(tif, tx + ty*num_tiles_x,
                                 pb.GetRawData(), tile_size);
        }
    }
    wcerr << endl;
}

std::array<double, 16>
init_model_transformation(GVGMap *map, const GVGMapInfo *mapinfo) {
    double rotx_cos = mapinfo->RADX_cos;
    double rotx_sin = mapinfo->RADX_sin;
    double roty_cos = mapinfo->RADY_cos;
    double roty_sin = mapinfo->RADY_sin;
    double scale_x = mapinfo->WPPX;
    double scale_y = mapinfo->WPPY;
    std::array<double, 16> model_transformation = {
        +rotx_cos*scale_x,  -rotx_sin*scale_x,  0,  mapinfo->WorldOrgX,
        +roty_sin*scale_y,  +roty_cos*scale_y,  0,  mapinfo->WorldOrgY,
                        0,                  0,  0,                   0,
                        0,                  0,  0,                   1};

    // GVG images set the origin to the bottom-left corner of the image,
    // while TIF wants it at the top-left corner. Compensate by mapping
    // y to (height - 1 - y).
    model_transformation[3] += (map->GetHeight()-1) * model_transformation[1];
    model_transformation[7] += (map->GetHeight()-1) * model_transformation[5];
    model_transformation[1] = -model_transformation[1];
    model_transformation[5] = -model_transformation[5];
    return model_transformation;
}

bool set_geotiff_keys(GVGMap *map, TIFF *tif) {
    int epsg, unit;
    auto mapinfo = map->GetGVGMapInfo();
    if (mapinfo->Projection == L"utm") {
        if (mapinfo->Ellipsoid != L"wgs84") {
            wcerr << L"The map uses an UTM projection with the ellipsoid "
                  << L"'" << mapinfo->Ellipsoid << L"'." << endl;
            wcerr << L"This combination is currently not supported." << endl;
            return false;
        }
        // FIXME: Handle southern hemisphere maps.
        epsg = 32600 + mapinfo->Zone;
        unit = Linear_Meter;
    } else if (mapinfo->Projection == L"gk") {
        if (mapinfo->Ellipsoid != L"bessel") {
            wcerr << L"The map uses a Gauss-Krueger projection with ellipsoid "
                  << L"'" << mapinfo->Ellipsoid << L"'." << endl;
            wcerr << L"This combination is currently not supported." << endl;
            return false;
        }
        if (mapinfo->Zone < 2 || mapinfo->Zone > 5) {
            wcerr << L"The map uses Gauss-Krueger zone "
                  << mapinfo->Zone << L"." << endl;
            wcerr << L"Only the zones 2 to 5 are supported!" << endl;
            return false;
        }
        epsg = 31464 + mapinfo->Zone;
        unit = Linear_Meter;
    } else {
        wcerr << L"Unknown map projection '" << mapinfo->Projection << L"'."
              << endl;
        return false;
    }

    GTIF_unique_ptr gtif_up(GTIFNew(tif), GTIF_Deleter);
    GTIF *gtif = gtif_up.get();
    if (!gtif) {
        wcerr << L"Failed to write geographic metadata to output file."
              << endl;
        return false;
    }

    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelTypeProjected);
    GTIFKeySet(gtif, ProjLinearUnitsGeoKey, TYPE_SHORT, 1, unit);
    GTIFKeySet(gtif, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, epsg);

    auto transformation = init_model_transformation(map, mapinfo);
    TIFFSetField(tif, TIFFTAG_GEOTRANSMATRIX, 16, transformation.data());

    if (map->GetType() == GeoDrawable::TYPE_DHM) {
        GTIFKeySet(gtif, VerticalUnitsGeoKey, TYPE_SHORT, 1, Linear_Meter);
    }

    return true;
}


int wmain(int argc, wchar_t* argv[]) {
    std::wstring user_output_fname;
    bool info_only = false;
    struct compression_def comp = {
            COMPRESSION_PACKBITS,
            JPEGCOLORMODE_RGB,
            DEFAULT_JPEG_COMPRESSION,
            0,
    };

    int c;
    while ((c = getopt(argc, argv, L"c:o:hi")) != -1) {
        switch (c) {
        case 'c':
            parse_compress_opts(optarg, &comp);
            break;
        case 'o':
            user_output_fname = optarg;
            break;
        case 'i':
            info_only = true;
            break;
        case 'h':
            usage();
        default:
            break;
        }
    }

    if (argc - optind < 1)
        usage();

    if (user_output_fname.size() && (argc - optind > 1)) {
        wcerr << L"The -o option can't be used with multiple input files."
              << endl;
        return 1;
    }

    if (user_output_fname.size() && info_only) {
        wcerr << L"The -o and -i options can't be used together." << endl;
        return 1;
    }

    int exitval = 0;
    while (optind < argc) {
        std::wstring ifname = argv[optind++];
        wcerr << L"Processing '" << ifname << L"'" << endl;
        auto map = std::unique_ptr<GVGMap>();
        try {
            map.reset(new GVGMap(ifname));
        } catch (const std::runtime_error &) {
            wcerr << L"Could not open input file: " << L"'" << ifname << L"'"
                  << endl;
            exitval |= 1;
            break;
        }

        if (info_only) {
            wcout << map->GetGVGFile().RawDataString() << endl;
            wcout << map->GetGMPImage().DebugData() << endl;
            break;
        }

        auto ofname = user_output_fname.size() ? user_output_fname :
                      ifname.substr(0, ifname.find_last_of(L".")) + L".tif";
        TIFF_unique_ptr tif(XTIFFOpenW(ofname.c_str(), "w"), &XTIFFClose);
        if (!tif) {
            wcerr << L"Could not open output file: " << L"'" << ofname << L"'"
                  << endl;
            exitval |= 1;
            break;
        }

        set_tiff_keys(map.get(), tif.get(), &comp);
        copy_tiles(map.get(), tif.get());
        if (!set_geotiff_keys(map.get(), tif.get())) {
            exitval |= 1;
        }
    }
    return exitval;
}

// The rest of the file is argument parsing code taken from bmp2tiff.c.
static void parse_compress_opts(wchar_t* opt, compression_def *comp) {
    if (wcscmp(opt, L"none") == 0)
        comp->compression = COMPRESSION_NONE;
    else if (wcscmp(opt, L"packbits") == 0)
        comp->compression = COMPRESSION_PACKBITS;
    else if (wcsncmp(opt, L"jpeg", 4) == 0) {
        wchar_t* cp = wcschr(opt, L':');

        comp->compression = COMPRESSION_JPEG;
        while(cp) {
            if (isdigit((int)cp[1])) {
                comp->jpegquality = _wtoi(cp+1);
            } else if (cp[1] == 'r' ) {
                comp->jpegcolormode = JPEGCOLORMODE_RAW;
            } else {
                wcerr << L"Invalid JPEG compression spec: "
                      << L"'" << opt << L"'" << endl;
                usage();
            }

            cp = wcschr(cp+1,L':');
        }
    } else if (wcsncmp(opt, L"lzw", 3) == 0) {
        wchar_t* cp = wcschr(opt, L':');
        if (cp)
            comp->predictor = _wtoi(cp+1);
        comp->compression = COMPRESSION_LZW;
    } else if (wcsncmp(opt, L"zip", 3) == 0) {
        wchar_t* cp = wcschr(opt, L':');
        if (cp)
            comp->predictor = _wtoi(cp+1);
        comp->compression = COMPRESSION_DEFLATE;
    } else {
        wcerr << L"Unknown compression method: "
              << L"'" << opt << L"'" << endl;
        usage();
    }
}

static const wchar_t* const usage_lines[] = {
L"gvg2geotiff --- Convert GVG/GMP maps to GeoTIFF",
L"Usage: gvg2geotiff [options] input.gvg [input2.gvg ...]",
L"Available options:",
L" -h                Show this help message",
L" -i                Show information about the map and exit",
L" -o out.tif        Write output to out.tif",
L" -c lzw[:opts]     Compress output with LZW encoding",
L" -c zip[:opts]     Compress output with deflate encoding",
L" -c jpeg[:opts]    Compress output with JPEG encoding",
L" -c packbits       Compress output with packbits encoding",
L" -c none           Do not use output compression",
L"",
L"JPEG options:",
L" #     Set compression quality level (0-100, default 75)",
L" r     Output color image as RGB rather than YCbCr",
L"Example: -c jpeg:r:65 gives JPEG-encoded RGB data with quality level 65",
L"",
L"LZW and deflate options:",
L" #     Set predictor value",
L"Example: -c lzw:2 gives LZW-encoded data with horizontal differencing",
NULL
};

static void usage(void) {
    for (int i = 0; usage_lines[i] != NULL; i++)
        wcerr << usage_lines[i] << std::endl;
    exit(-1);
}


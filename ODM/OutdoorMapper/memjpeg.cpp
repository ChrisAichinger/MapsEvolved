#include "memjpeg.h"

#include <memory>
#include <stdexcept>

extern "C" {
#include "jpeglib.h"
#include "jerror.h"
#include "setjmp.h"
}


class JPEGError : public std::runtime_error {
    public:
        explicit JPEGError(const std::string &_Message)
            : std::runtime_error(_Message.c_str())
            { }

        explicit JPEGError(const char *_Message)
            : std::runtime_error(_Message)
            { }
};

METHODDEF(void)
error_exit_handler(j_common_ptr cinfo) {
    (*cinfo->err->output_message)(cinfo);
    throw JPEGError("Failed to decompress JPEG buffer.");
}

PixelBuf decompress_jpeg(const std::string &buf, bool swap_rb) {
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr err_mgr;

    // Set up the normal JPEG error routines, then override error_exit.
    cinfo.err = jpeg_std_error(&err_mgr);
    err_mgr.error_exit = error_exit_handler;

    // Attach an unique_ptr directly after creation, to ensure
    // jpeg_destroy_decompress() is called in case an exception occurrs.
    jpeg_create_decompress(&cinfo);
    auto deleter = [](j_decompress_ptr p){ jpeg_destroy_decompress(p); };
    std::unique_ptr<jpeg_decompress_struct, decltype(deleter)>
            cleanup_cinfo(&cinfo, deleter);

    jpeg_mem_src(&cinfo, (JOCTET*)buf.c_str(), buf.size());
    jpeg_read_header(&cinfo, /* require_image = */ true);

    // Ensure we get 24 bit RGB or an error, without explicitly setting the
    // out_color_space, we might get grayscale or CMYK data.
    // Libjpeg can do the color conversions if necessary.
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        // This should never happen, as we explicitly set JCS_RGB and libjpeg
        // should honor it. We might get a nasty buffer overflow if it doesn't,
        // so guard against it.
        throw JPEGError("Invalid number of JPEG color components.");
    }
    int row_stride = cinfo.output_width * cinfo.output_components;
    auto output = PixelBuf(cinfo.output_width, cinfo.output_height);
    auto output_buf = reinterpret_cast<unsigned char*>(output.GetRawData());

    // PixelBuf works on 32-bit RGBX values, while libjpeg can only provide 24
    // bit packed RGB. Read that into the PixelBuf buffer first, then space it
    // out in a second step later.
    JSAMPROW rows[1];
    bool top_down = false;
    unsigned int pos = top_down ? 0 : row_stride * (cinfo.output_height - 1);
    int pos_delta = top_down ? row_stride : -row_stride;
    unsigned int pos_end = pos + cinfo.output_height * pos_delta;
    while (cinfo.output_scanline < cinfo.output_height && pos != pos_end) {
        // TODO: Use a larger output JSAMPARRAY and handle return value of
        // jpeg_read_scanlines(). Cf. cinfo.rec_outbuf_height
        rows[0] = &output_buf[pos];
        (void) jpeg_read_scanlines(&cinfo, rows, 1);
        pos += pos_delta;
    }

    jpeg_finish_decompress(&cinfo);

    // Convert 24 bit RGB to 32 bit RGBX, possibly exchange R and B channels.
    int R = swap_rb ? 2 : 0;
    int G = 1;
    int B = swap_rb ? 0 : 2;
    unsigned int num_pixels = output.GetWidth() * output.GetHeight();
    for (int i = num_pixels - 1; i >= 0; i--) {
        unsigned int val = (output_buf[3*i + R] << 0) |
                           (output_buf[3*i + G] << 8) |
                           (output_buf[3*i + B] << 16);
        *reinterpret_cast<unsigned int*>(&output_buf[4*i]) = val;
    }
    return output;
}
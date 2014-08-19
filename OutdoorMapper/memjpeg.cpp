#include "memjpeg.h"

#include <memory>
#include <stdexcept>

extern "C" {
#include "jpeglib.h"
#include "jerror.h"
#include "setjmp.h"
}

GLOBAL(void) jpeg_mem_src(j_decompress_ptr cinfo,
                          JOCTET * pData,
                          int FileSize,
                          void *pDataSrc,
                          JMETHOD(void, notifyCppWorld, (j_common_ptr)));


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

void progress_cb(j_common_ptr) {
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

    jpeg_mem_src(&cinfo, (JOCTET*)buf.c_str(), buf.size(), NULL, &progress_cb);
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

// The rest of the file is copied from jmemsrc.c.
// It is not part of libjpeg 6b-4, so include it here.
// When switching to libjpeg-turbo, we can delete it.

/*
 * jmemsrc.c
 *
 * This file contains a decompression data source which takes a
 * memory region as source.
 * The only thing these routines really do is tell the library where the
 * data is on construction (jpeg_mem_src()). Everything else is there
 * for error checking purposes.
 * Adapted from jdatasrc.c 9/96 by Ulrich von Zadow.
 */

/* this is not a core library module, so it doesn't define JPEG_INTERNALS */



/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * If this procedure gets called, we have a buffer overrun condition -
 * there is not enough data in the buffer to satisfy the decoder.
 * The procedure just generates a warning and feeds the decoder a fake
 * JPEG_EOI marker.
 */
METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr * src = cinfo->src;
  static JOCTET FakeEOI[] = { 0xFF, JPEG_EOI };

  /* Generate warning */
  WARNMS(cinfo, JWRN_JPEG_EOF);

  /* Insert a fake EOI marker */
  src->next_input_byte = FakeEOI;
  src->bytes_in_buffer = 2;

  return TRUE;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  struct jpeg_source_mgr * src = cinfo->src;

  if(num_bytes >= (long)src->bytes_in_buffer)
  {
    fill_input_buffer(cinfo);
    return;
  }

  src->bytes_in_buffer -= num_bytes;
  src->next_input_byte += num_bytes;
}


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}


/*
 * Prepare for input. This routine tells the jpeg library where to find
 * the data & sets up the function pointers the library needs.
 *
 * Jo Hagelberg 15.4.99
 * added pDataSrc and JMETHOD( notifyCppworld ) for progress notification
 */

GLOBAL(void)
jpeg_mem_src (
              j_decompress_ptr cinfo,
              JOCTET * pData,
              int FileSize,
              void *pDataSrc,
              JMETHOD(void, notifyCppWorld, (j_common_ptr))
             )
{
  struct jpeg_source_mgr * src;

  if (cinfo->src == NULL)
  {   /* first time for this JPEG object? */
      cinfo->src = (struct jpeg_source_mgr *)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                            sizeof(struct jpeg_source_mgr));
  }

  /* Jo Hagelberg 15.4.99
   * added progress notification for JPEG
   */
  if ( cinfo->progress == NULL)
  {   /* first time and progress notification wanted? */
      cinfo->progress = (struct jpeg_progress_mgr *)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                            sizeof(struct jpeg_progress_mgr));
      /* static callback function in JpegDecoder */
      cinfo->progress->progress_monitor = notifyCppWorld;
  }

  /* Jo Hagelberg 15.4.99
   * put CDataSource instance ptr into client data, so we can use it in the callback
   * NOTE: define a client data struct if this should be needed for other stuff, too
   */
  cinfo->client_data = pDataSrc ;

  src = cinfo->src;

  /* Set up function pointers */
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;

  /* Set up data pointer */
  src->bytes_in_buffer = FileSize;
  src->next_input_byte = pData;
}
/*
/--------------------------------------------------------------------
|
|      $Log: jmemsrc.cpp,v $
|      Revision 1.4  2004/09/11 12:41:34  uzadow
|      removed plstdpch.h
|
|      Revision 1.3  2001/09/16 20:57:17  uzadow
|      Linux version name prefix changes
|
|      Revision 1.2  2001/09/15 21:02:44  uzadow
|      Cleaned up stdpch.h and config.h to make them internal headers.
|
|      Revision 1.1  2000/10/30 14:32:50  uzadow
|      Removed dependency on jinclude.h
|
|      Revision 1.5  2000/09/01 13:27:07  Administrator
|      Minor bugfixes
|
|
\--------------------------------------------------------------------
*/

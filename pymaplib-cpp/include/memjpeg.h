#ifndef ODM__MEMJPEG_H
#define ODM__MEMJPEG_H

#include <string>

#include "odm_config.h"
#include "pixelbuf.h"

// Generate a PixelBuf from an in-memory jpeg ``buf``.
// If ``swap_rb`` is true, the red and blue channels are swapped.
// This is usually not needed, however, it is required for GVG maps.
EXPORT PixelBuf decompress_jpeg(const std::string &buf, bool swap_rb=false);

#endif

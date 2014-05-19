#include "pixelbuf.h"

#include <assert.h>

#include "util.h"

PixelBuf::PixelBuf(int width, int height)
    // Zero-initialize the memory block (notice the parentheses).
    : m_data(std::shared_ptr<unsigned int>(new unsigned int[width*height](),
                                           ArrayDeleter<unsigned int>())),
      m_width(width), m_height(height)
{ }

PixelBuf::PixelBuf(const std::shared_ptr<unsigned int> &data,
                   int width, int height)
    : m_data(data), m_width(width), m_height(height)
{ }

void PixelBuf::Insert(unsigned int x_start,
                       unsigned int y_start,
                       const PixelBuf &source)
{
    unsigned int x_end = std::min(x_start + source.GetWidth(), m_width);
    unsigned int y_end = std::min(y_start + source.GetHeight(), m_height);
    unsigned int x_delta = x_end - x_start;
    unsigned int y_delta = y_end - y_start;
    for (unsigned int y = 0; y < y_delta; ++y) {
        unsigned int *dest = GetPixelPtr(x_start, y_start + y);
        const unsigned int *src = source.GetPixelPtr(0, y);
        assert(dest >= GetRawData());
        assert(dest + x_delta <= &GetRawData()[m_width*m_height]);
        memcpy(dest, src, x_delta * sizeof(unsigned int));
    }
}
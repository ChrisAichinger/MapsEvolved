#include "pixelbuf.h"

#include <assert.h>

#include "util.h"

PixelBuf::PixelBuf(int width, int height)
    // Zero-initialize the memory block (notice the parentheses).
    : m_data(std::shared_ptr<unsigned int>(new unsigned int[width*height](),
                                           ArrayDeleter<unsigned int>())),
      m_width(width), m_height(height)
{ }

PixelBuf::PixelBuf(int width, int height, unsigned int value)
    // Zero-initialize the memory block (notice the parentheses).
    : m_data(std::shared_ptr<unsigned int>(new unsigned int[width*height],
                                           ArrayDeleter<unsigned int>())),
      m_width(width), m_height(height)
{
    auto buf = m_data.get();
    for (int i=0; i < width*height; i++) {
        buf[i] = value;
    }
}

PixelBuf::PixelBuf(const std::shared_ptr<unsigned int> &data,
                   int width, int height)
    : m_data(data), m_width(width), m_height(height)
{ }

void PixelBuf::Insert(int x_start,
                      int y_start,
                      const PixelBuf &source)
{
    int x_dst_start = std::max(x_start, 0);
    int y_dst_start = std::max(y_start, 0);
    int x_src_offset = x_dst_start - x_start;
    int y_src_offset = y_dst_start - y_start;
    int x_dst_end = std::min(x_start + source.GetWidth(), m_width);
    int y_dst_end = std::min(y_start + source.GetHeight(), m_height);
    int x_delta = x_dst_end - x_dst_start;
    int y_delta = y_dst_end - y_dst_start;
    if (x_delta <= 0 || y_delta <= 0) {
        return;
    }
    for (int y = 0; y < y_delta; ++y) {
        unsigned int *dest = GetPixelPtr(x_dst_start, y + y_dst_start);
        const unsigned int *src = source.GetPixelPtr(x_src_offset, y + y_src_offset);
        assert(dest >= GetRawData());
        assert(dest + x_delta <= &GetRawData()[m_width*m_height]);
        memcpy(dest, src, x_delta * sizeof(unsigned int));
    }
}
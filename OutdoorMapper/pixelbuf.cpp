#include "pixelbuf.h"

#include <assert.h>

#include "util.h"
#include "coordinates.h"

PixelBuf::PixelBuf(int width, int height)
    // Zero-initialize the memory block (notice the parentheses).
    : m_data(std::shared_ptr<unsigned int>(new unsigned int[width*height](),
                                           ArrayDeleter<unsigned int>())),
      m_width(width), m_height(height)
{
    if (width < 0 || height < 0) {
        throw std::runtime_error(
            "PixelBuf width and height must be positive.");
    }
}

PixelBuf::PixelBuf(int width, int height, unsigned int value)
    : m_data(std::shared_ptr<unsigned int>(new unsigned int[width*height],
                                           ArrayDeleter<unsigned int>())),
      m_width(width), m_height(height)
{
    if (width < 0 || height < 0) {
        throw std::runtime_error(
            "PixelBuf width and height must be positive.");
    }
    auto buf = m_data.get();
    for (int i=0; i < width*height; i++) {
        buf[i] = value;
    }
}

PixelBuf::PixelBuf(const std::shared_ptr<unsigned int> &data,
                   int width, int height)
    : m_data(data), m_width(width), m_height(height)
{
    if (width < 0 || height < 0) {
        throw std::runtime_error(
            "PixelBuf width and height must be positive.");
    }
}

void PixelBuf::Insert(const PixelBufCoord &pos, const PixelBuf &source) {
    int x_dst_start = std::max(pos.x, 0);
    int y_dst_start = std::max(pos.y, 0);
    int x_src_offset = x_dst_start - pos.x;
    int y_src_offset = y_dst_start - pos.y;
    int x_dst_end = std::min(pos.x + source.GetWidth(), m_width);
    int y_dst_end = std::min(pos.y + source.GetHeight(), m_height);
    int x_delta = x_dst_end - x_dst_start;
    int y_delta = y_dst_end - y_dst_start;
    if (x_delta <= 0 || y_delta <= 0) {
        return;
    }
    for (int y = 0; y < y_delta; ++y) {
        auto dest = GetPixelPtr(x_dst_start, y + y_dst_start);
        auto src = source.GetPixelPtr(x_src_offset, y + y_src_offset);
        assert(dest >= GetRawData());
        assert(dest + x_delta <= &GetRawData()[m_width*m_height]);
        memcpy(dest, src, x_delta * sizeof(*dest));
    }
}

void PixelBuf::SetPixel(const PixelBufCoord &pos, unsigned int val) {
    // We ensure (int)m_width/(int)m_height >= 0 in the c'tors.
    if (pos.x >= 0 && pos.x < static_cast<int>(m_width) &&
        pos.y >= 0 && pos.y < static_cast<int>(m_height))
    {
        GetRawData()[pos.x + (m_height - pos.y - 1) * m_width] = val;
    }
}

void PixelBuf::Line(const PixelBufCoord &start,
                    const PixelBufCoord &end,
                    const unsigned int color)
{
    int x1 = start.x;
    int x2 = end.x;
    int y1 = start.y;
    int y2 = end.y;

    // Bresenham's line algorithm
    const bool is_steep = (abs(y2 - y1) > abs(x2 - x1));
    if (is_steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }

    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    const int dx = x2 - x1;
    const int dy = abs(y2 - y1);
    int error = dx;
    const int ystep = (y1 < y2) ? 1 : -1;
    int y = y1;

    for (int x = x1; x < x2; x++) {
        if (is_steep) {
            SetPixel(PixelBufCoord(y, x), color);
        } else {
            SetPixel(PixelBufCoord(x, y), color);
        }

        error -= 2 * dy;
        if (error < 0) {
            y += ystep;
            error += 2 * dx;
        }
    }
}

void PixelBuf::Rect(const PixelBufCoord &start,
                    const PixelBufCoord &end,
                    const unsigned int color)
{
    for (int y = start.y; y < end.y; y++) {
        for (int x = start.x; x < end.x; x++) {
            SetPixel(PixelBufCoord(x, y), color);
        }
    }
}
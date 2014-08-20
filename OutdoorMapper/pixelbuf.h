#ifndef ODM__PIXELFORMAT_H
#define ODM__PIXELFORMAT_H

#include <memory>

#include "odm_config.h"

enum ODMPixelFormat {
    ODM_PIX_INVALID = 0,
    ODM_PIX_RGBA4,
    ODM_PIX_RGBX4,
};

class EXPORT PixelBuf {
    public:
        PixelBuf() : m_data(), m_width(0), m_height(0) {};
        PixelBuf(int width, int height);
        PixelBuf(int width, int height, unsigned int value);
        PixelBuf(const std::shared_ptr<unsigned int> &data,
                 int width, int height);

        inline std::shared_ptr<unsigned int> &GetData() { return m_data; }
        inline unsigned int * GetRawData() { return m_data.get(); }
        inline const unsigned int * GetRawData() const { return m_data.get(); }
        inline unsigned int GetWidth() const { return m_width; }
        inline unsigned int GetHeight() const { return m_height; }
        inline unsigned int *GetPixelPtr(int x, int y) {
            return &m_data.get()[x + y*m_width];
        }
        inline const unsigned int *GetPixelPtr(int x, int y) const {
            return &m_data.get()[x + y*m_width];
        }
        inline unsigned int GetPixel(int x, int y) const {
            return m_data.get()[x + y*m_width];
        }

        void Insert(const class PixelBufCoord &pos, const PixelBuf &source);
        void SetPixel(const class PixelBufCoord &pos, unsigned int val);
        void Line(const class PixelBufCoord &start,
                  const class PixelBufCoord &end,
                  const unsigned int color);
        void Rect(const class PixelBufCoord &start,
                  const class PixelBufCoord &end,
                  const unsigned int color);
    private:
        std::shared_ptr<unsigned int> m_data;
        unsigned int m_width;
        unsigned int m_height;
};


#endif

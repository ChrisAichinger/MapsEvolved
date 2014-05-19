#ifndef ODM__PIXELFORMAT_H
#define ODM__PIXELFORMAT_H

#include <memory>

enum ODMPixelFormat {
    ODM_PIX_RGBA4,
    ODM_PIX_RGBX4,
};

class EXPORT MapRegion {
    public:
        MapRegion() : m_data(), m_width(0), m_height(0) {};
        MapRegion(int width, int height);
        MapRegion(const std::shared_ptr<unsigned int> &data,
                  int width, int height)
            : m_data(data), m_width(width), m_height(height) {};
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
        void Insert(unsigned int x, unsigned int y, const MapRegion &source);
    private:
        std::shared_ptr<unsigned int> m_data;
        unsigned int m_width;
        unsigned int m_height;
};


#endif

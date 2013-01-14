#ifndef ODM__UTIL_H
#define ODM__UTIL_H

#include <string>
#include <memory>


#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)


template <typename T>
class ArrayDeleter {
    public:
        void operator () (T* d) const {
            delete [] d;
        }
};


template <typename T>
class FreeDeleter {
    public:
        void operator () (T* d) const {
            if (d) free(d);
        }
};

int round_to_neg_inf(double value);
int round_to_neg_inf(int value, int round_to);
int round_to_neg_inf(double value, int round_to);
int round_to_int(double r);

inline double lerp(double factor, double v1, double v2) {
    return v1 + factor * (v2 - v1);
}


template <typename T>
T ValueBetween(T v_min, T value, T v_max) {
    if (value < v_min) return v_min;
    if (value > v_max) return v_max;
    return value;
}

bool ends_with(const std::wstring &fullString, const std::wstring &ending);
std::string UTF8FromWString(const std::wstring &string);
std::wstring WStringFromUTF8(const std::string &string);

// Array size helper and macro
template <size_t N> struct ArraySizeHelper { char _[N]; };
template <typename T, size_t N>
ArraySizeHelper<N> makeArraySizeHelper(T(&)[N]);
#define ARRAY_SIZE(a)  sizeof(makeArraySizeHelper(a))

inline
unsigned int makeRGB(unsigned char r, unsigned char g, unsigned char b) {
    return r + (g << 8) + (b << 16);
}

unsigned int HSV_to_RGB(unsigned char H, unsigned char S, unsigned char V);

/*
unsigned int Elem(unsigned int x, unsigned int y, unsigned int width) {
    return x + y * width;
}
*/

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

struct BasicBitmap {
    unsigned int width, height, bpp;
    std::shared_ptr<unsigned int> pixels;
};
BasicBitmap LoadBufferFromBMP(const std::wstring &fname);
void SaveBufferAsBMP(const std::wstring &fname, void *buffer,
                     unsigned int width, unsigned int height,
                     unsigned int bpp);

#endif

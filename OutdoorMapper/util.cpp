#include <cmath>
#include <cassert>
#include <climits>
#include <string>
#include <locale>
#include <codecvt>
#include <iostream>
#include <fstream>

#include <Windows.h>

#include "util.h"


int round_to_neg_inf(int value, int round_to) {
    if (value >= 0) {
        return (value / round_to) * round_to;
    } else {
        return ((value - round_to + 1) / round_to) * round_to;
    }
}

int round_to_neg_inf(double value) {
    double floorval = std::floor(value);
    assert(INT_MIN <= floorval && floorval <= INT_MAX);
    return (int)floorval;
}

int round_to_neg_inf(double value, int round_to) {
    int intval = (int)round_to_neg_inf(value);
    return round_to_neg_inf(intval, round_to);
}

int round_to_int(double r) {
    return static_cast<int>((r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5));
}

bool ends_with(const std::wstring &fullString, const std::wstring &ending) {
    if (fullString.length() >= ending.length()) {
        return 0 == fullString.compare(fullString.length() - ending.length(),
                                       ending.length(), ending);
    } else {
        return false;
    }
}

std::string UTF8FromWString(const std::wstring &string) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t> convert;
    return convert.to_bytes(string);
}

std::wstring WStringFromUTF8(const std::string &string) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
    return convert.from_bytes(string);
}

unsigned int HSV_to_RGB(unsigned char H, unsigned char S, unsigned char V) {
    unsigned char hi = H * 6 / 255;
    unsigned char f = H * 6 - hi * 255;
    unsigned char p = V - V*S / 255;
    unsigned char q = V - V*S*f / (255*255);
    unsigned char t = V - V*S / 255 + V*S*f / (255*255);

    switch (hi) {
        case 0: // fallthrough
        case 6:
            return makeRGB(V, t, p);
        case 1:
            return makeRGB(q, V, p);
        case 2:
            return makeRGB(p, V, t);
        case 3:
            return makeRGB(p, q, V);
        case 4:
            return makeRGB(t, p, V);
        case 5:
            return makeRGB(V, p, q);
    }
    assert(false);
    return 0;
}

BasicBitmap LoadBufferFromBMP(const std::wstring &fname) {
    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    std::ifstream file(fname, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening BMP file for reading.");
    }
    file.read(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    file.read(reinterpret_cast<char*>(&bih), sizeof(bih));
    if (bih.biBitCount != 32 || bih.biCompression != BI_RGB) {
        throw std::runtime_error("Format of  the BMP file is not supported.");
    }

    std::shared_ptr<unsigned int> buffer(
            new unsigned int[bih.biWidth * bih.biHeight],
            ArrayDeleter<unsigned int>());
    file.read(reinterpret_cast<char*>(buffer.get()), bih.biWidth * bih.biHeight * bih.biBitCount / 8);
    BasicBitmap res = { bih.biWidth, bih.biHeight, bih.biBitCount, buffer };
    return res;
}

void SaveBufferAsBMP(const std::wstring &fname, void *buffer,
                     unsigned int width, unsigned int height, unsigned int bpp)
{
    static const unsigned short int BMP_MAGIC = 0x4D42;
    static const unsigned int SZ_BMP_HDR = sizeof(BITMAPFILEHEADER) +
                                           sizeof(BITMAPINFOHEADER);
    BITMAPFILEHEADER bfh = { BMP_MAGIC, SZ_BMP_HDR + width * height * bpp / 8,
                             0, 0, SZ_BMP_HDR };
    BITMAPINFOHEADER bih = { sizeof(BITMAPINFOHEADER),
                             width, height, 1, bpp, BI_RGB, 0, 0, 0, 0, 0 };
    std::ofstream file(fname, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening BMP file for writing.");
    }
    file.write(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    file.write(reinterpret_cast<char*>(&bih), sizeof(bih));
    file.write(reinterpret_cast<char*>(buffer), width * height * bpp / 8);
    file.close();
}


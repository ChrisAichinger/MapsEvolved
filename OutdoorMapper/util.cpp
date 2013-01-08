#include <cmath>
#include <cassert>
#include <climits>
#include <string>
#include <locale>
#include <codecvt>

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

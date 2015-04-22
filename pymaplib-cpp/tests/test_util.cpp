#include <string>
#include <iostream>

#include "util.h"
#include "test.h"

int test_util() {
    test_eq(normalize_direction(0.0), 0.0);
    test_eq(normalize_direction(360.0), 0.0);
    test_eq(normalize_direction(361.0), 1.0);

    test_eq(string_format(L"a: %d", 9), std::wstring(L"a: 9"));
    test_eq(string_format(L"b: %d", 99), std::wstring(L"b: 99"));
    test_eq(string_format(L"c: %s", L"test"), std::wstring(L"c: test"));

    for (int i = 256; i <= 4096; i*= 2) {
        std::wstring str_im1;
        str_im1.resize(i-1, 'X');
        test_eq(string_format(L"%s", str_im1.c_str()), str_im1);

        std::wstring str_i;
        str_i.resize(i, 'X');
        test_eq(string_format(L"%s", str_i.c_str()), str_i);

        std::wstring str_ip1;
        str_ip1.resize(i+1, 'X');
        test_eq(string_format(L"%s", str_ip1.c_str()), str_ip1);
    }
    return 0;
}

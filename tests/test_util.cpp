#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>

#include "util.h"

void test_imp(bool v, const char* explanation) {
    if (v) {
        printf("[OK] %s\n", explanation);
    } else {
        printf("FAIL %s\n", explanation);
        assert(false);
    }
}

template <typename T, typename U>
void test_op(bool res, const char* op, T a, U b, const char* a_str, const char* b_str) {
    if (res) {
        std::wcout << "[OK] " << a_str << " " << op << " " << b_str << std::endl;
    } else {
        std::wcout << "FAIL " << a_str << " " << op << " " << b_str << std::endl;
        std::wcout << "     lhs is " << a << std::endl;
        std::wcout << "     rhs is " << b << std::endl;
        assert(false);
    }
}

#define test(v) test_imp((v), std::string(#v))
#define test_eq(v, w) test_op((v) == (w), "==", (v), (w), #v, #w)

int main() {
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

    printf("\nFinished\n");
    return 0;
}

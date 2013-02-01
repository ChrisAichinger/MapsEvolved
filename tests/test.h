#ifndef ODM__TEST_H
#define ODM__TEST_H

#include <string>
#include <iostream>
#include <cassert>

template <typename T, typename U>
void test_op(bool res, const char* op, T a, U b,
             const char* a_str, const char* b_str)
{
    if (res) {
        std::wcout << "[OK] " << a_str << " " << op << " " << b_str << std::endl;
    } else {
        std::wcout << "FAIL " << a_str << " " << op << " " << b_str << std::endl;
        std::wcout << "     lhs is " << a << std::endl;
        std::wcout << "     rhs is " << b << std::endl;
        assert(false);
    }
}

#define test_eq(v, w) test_op((v) == (w), "==", (v), (w), #v, #w)


int test_util();

typedef int (*TestFunction)();
static TestFunction test_funcs[] = { test_util, 0 };


#endif

#include <string>
#include <iostream>

#include "../include/util.h"

#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(util)
{
    //BOOST_CHECK_EQUAL(normalize_direction(0.0), 0.0);
    //BOOST_CHECK_EQUAL(normalize_direction(360.0), 0.0);
    //BOOST_CHECK_EQUAL(normalize_direction(361.0), 1.0);

    //BOOST_CHECK_EQUAL(string_format(L"a: %d", 9), std::wstring(L"a: 9"));
    //BOOST_CHECK_EQUAL(string_format(L"b: %d", 99), std::wstring(L"b: 99"));
    //BOOST_CHECK_EQUAL(string_format(L"c: %s", L"test"), std::wstring(L"c: test"));

    for (int i = 256; i <= 4096; i*= 2) {
        std::wstring str_im1;
        str_im1.resize(i-1, 'X');
        //BOOST_CHECK_EQUAL(string_format(L"%s", str_im1.c_str()), str_im1);

        std::wstring str_i;
        str_i.resize(i, 'X');
        //BOOST_CHECK_EQUAL(string_format(L"%s", str_i.c_str()), str_i);

        std::wstring str_ip1;
        str_ip1.resize(i+1, 'X');
        //BOOST_CHECK_EQUAL(string_format(L"%s", str_ip1.c_str()), str_ip1);
    }
}

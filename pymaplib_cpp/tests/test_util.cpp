// Copyright 2015 Christian Aichinger <Greek0@gmx.net>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <iostream>

#include "../include/util.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(util)

BOOST_AUTO_TEST_CASE(norm_direction)
{
    BOOST_CHECK_CLOSE(normalize_direction(0.0), 0.0,   0.1);
    BOOST_CHECK_CLOSE(normalize_direction(360.0), 0.0, 0.1);
    BOOST_CHECK_CLOSE(normalize_direction(361.0), 1.0, 0.1);
}

BOOST_AUTO_TEST_CASE(wstring_format)
{
    BOOST_CHECK(string_format(L"a: %d", 9) == L"a: 9");
    BOOST_CHECK(string_format(L"b: %d", 99) == L"b: 99");
    BOOST_CHECK(string_format(L"c: %s", L"test") == L"c: test");

    for (int i = 256; i <= 4096; i*= 2) {
        std::wstring str_im1;
        str_im1.resize(i-1, 'X');
        BOOST_CHECK(string_format(L"%s", str_im1.c_str()) == str_im1);

        std::wstring str_i;
        str_i.resize(i, 'X');
        BOOST_CHECK(string_format(L"%s", str_i.c_str()) == str_i);

        std::wstring str_ip1;
        str_ip1.resize(i+1, 'X');
        BOOST_CHECK(string_format(L"%s", str_ip1.c_str()) == str_ip1);
    }
}

BOOST_AUTO_TEST_SUITE_END()

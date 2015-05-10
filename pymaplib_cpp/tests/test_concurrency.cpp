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
#include <cstring>

#include "../include/rastermap.h"
#include "../include/util.h"

#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/chrono/include.hpp>

#include "tests.h"

#define memeq(a, b, sz) (0 == memcmp((a), (b), (sz)))

boost::mutex boost_test_mutex;
#define THREADSAFE_CHECK(x)                                     \
    if (bool not_x = !(x)) {                                    \
        boost::lock_guard<boost::mutex> lock(boost_test_mutex); \
        BOOST_CHECK(!not_x);                                    \
    }

BOOST_AUTO_TEST_SUITE(concurrency)

void test_getregion(const std::shared_ptr<const GeoDrawable> &map,
                    const PixelBuf &image, unsigned int duration_msecs)
{
    auto end_time = boost::chrono::system_clock::now() +
                    boost::chrono::milliseconds(duration_msecs);

    const auto img_data = image.GetRawData();
    const auto size = image.GetWidth() * image.GetHeight() * sizeof(*img_data);
    while (boost::chrono::system_clock::now() < end_time) {
        auto pixels = map->GetRegion(MapPixelCoordInt(0, 0), map->GetSize());
        THREADSAFE_CHECK(pixels.GetWidth() == image.GetWidth());
        THREADSAFE_CHECK(pixels.GetHeight() == image.GetHeight());
        THREADSAFE_CHECK(memeq(pixels.GetRawData(), img_data, size));
    }
}

static std::wstring get_test_tif() {
    return GetProgramDir_wchar() + L".." + ODM_PathSep_wchar +
                                   L".." + ODM_PathSep_wchar +
                                   L"tests" + ODM_PathSep_wchar +
                                   L"data" + ODM_PathSep_wchar +
                                   L"no-geoinfo.tif";
}

BOOST_AUTO_TEST_CASE(rastermap_getregion)
{
    // By default run for only 1 second to not slow down the tests.
    auto duration = testconfig.concurrency_test_msecs().value_or(100);
    auto map = LoadMap(get_test_tif());
    auto image = map->GetRegion(MapPixelCoordInt(0,0), map->GetSize());
    auto num_threads = boost::thread::hardware_concurrency();
    auto threads = std::vector<boost::thread>();
    for (auto i = 0U; i < num_threads; i++) {
        threads.push_back(boost::thread(test_getregion, map, image, duration));
    }
    for (auto it = threads.begin(); it != threads.end(); ++it) {
        it->join();
    }
}

BOOST_AUTO_TEST_CASE(rastermap_getregion_multiple_maps)
{
    // By default run for only 1 second to not slow down the tests.
    auto duration = testconfig.concurrency_test_msecs().value_or(100);
    auto mapfile = get_test_tif();
    auto map = LoadMap(mapfile);
    auto image = map->GetRegion(MapPixelCoordInt(0,0), map->GetSize());
    auto num_threads = boost::thread::hardware_concurrency();
    auto threads = std::vector<boost::thread>();
    for (auto i = 0U; i < num_threads; i++) {
        auto new_map = LoadMap(mapfile);
        threads.push_back(boost::thread(test_getregion, new_map, image,
                                        duration));
    }
    for (auto it = threads.begin(); it != threads.end(); ++it) {
        it->join();
    }
}

BOOST_AUTO_TEST_SUITE_END()

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

#include <cstdlib>
#include <iomanip>
#include <string>
#include <vector>

#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_MODULE Master Test Suite
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/debug.hpp>


struct test_tree_reporter : boost::unit_test::test_tree_visitor {
public:
    test_tree_reporter(std::ostream &stream)
        : m_indent(-4),      // Don't output the master_test_suite name.
          m_stream(stream)
    {}

private:
    virtual void visit(boost::unit_test::test_case const& tc) {
        m_stream << std::setw(m_indent) << "" << tc.p_name << "\n";
    }
    virtual bool test_suite_start(boost::unit_test::test_suite const& ts) {
        if( m_indent >= 0 ) {
            m_stream << std::setw(m_indent) << "" << ts.p_name << "\n";
        }
        m_indent += 4;
        return true;
    }
    virtual void test_suite_finish(boost::unit_test::test_suite const&) {
        m_indent -= 4;
    }

    int m_indent;
    std::ostream &m_stream;
};


static std::vector<std::string> args;
static bool want_test_list() {
    return args.size() == 1 && (args[0] == "-l" || args[0] == "--list");
}


static bool init_maplib_tests() {
    if (!init_unit_test()) {
        return false;
    }
    if (want_test_list()) {
        auto& stream = boost::unit_test::results_reporter::get_stream();
        stream << "Available test suites and cases:" << std::endl;

        auto p_id = boost::unit_test::framework::master_test_suite().p_id;
        test_tree_reporter content_reporter(stream);
        traverse_test_tree(p_id, content_reporter);

        // Disable memory leak detection when only showing the test case list.
        // Otherwise an ugly error is printed due to the std::exit() call.
        boost::debug::detect_memory_leaks(false);

        // Exit directly to suppress running tests or showing error messages.
        std::exit(0);
    }
    return true;
}


#ifdef BOOST_TEST_NO_MAIN
int main(int argc, char* argv[]) {
    args = std::vector<std::string>(argv + 1, argv + argc);
    int ret = boost::unit_test::unit_test_main(&init_maplib_tests, argc, argv);
    return ret;  // Conditional breakpoint here.
}
#endif

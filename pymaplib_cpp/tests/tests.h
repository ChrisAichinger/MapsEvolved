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
#include <set>
#include <cassert>

#include <boost/optional.hpp>

extern class TestConfig testconfig;
class TestConfig {
public:
    /** Add a single configuration entry. */
    void add(const std::string &s) {
        m_args.insert(s);
    }
    /** Add multiple configuration entries. */
    template <typename Iter> void add(const Iter &first, const Iter &last) {
        m_args.insert(first, last);
    }

    bool want_test_list() const;
    boost::optional<unsigned int> concurrency_test_msecs() const;

private:
    /** Initialization method to be called by main() */
    void initialize(int argc, char* argv[]);

    std::string m_progname;
    std::set<const std::string> m_args;

    friend int main(int, char*[]);
};


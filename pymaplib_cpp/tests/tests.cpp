#define BOOST_TEST_MODULE Master Test Suite
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( simple_test )
{
     BOOST_CHECK_EQUAL(1, 1);
     BOOST_CHECK(true);
}
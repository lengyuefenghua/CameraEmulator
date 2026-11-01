#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 test target starts", "[smoke]") {
    REQUIRE(2 + 2 == 4);
}

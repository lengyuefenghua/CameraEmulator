#include "protocol/gvsp_codec.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace {
std::byte b(unsigned value) { return static_cast<std::byte>(value); }
}

TEST_CASE("GVSP image leader and trailer match the standard fixed layout", "[GvspGolden]") {
    const camera::protocol::GvspFrameInfo info{0, 0x1234, 2, 1, camera::core::PixelFormat::Mono8};
    const auto leader = camera::protocol::GvspCodec::leader(info);
    REQUIRE(leader.size() == 44);
    REQUIRE(leader[0] == b(0));
    REQUIRE(leader[1] == b(0));
    REQUIRE(leader[2] == b(0x12));
    REQUIRE(leader[3] == b(0x34));
    REQUIRE(leader[4] == b(0x01));
    REQUIRE(leader[5] == b(0x00));
    REQUIRE(leader[6] == b(0x00));
    REQUIRE(leader[7] == b(0x00));
    REQUIRE(leader[10] == b(0x00));
    REQUIRE(leader[11] == b(0x01));
    REQUIRE(leader[27] == b(0x02));
    REQUIRE(leader[31] == b(0x01));

    const auto trailer = camera::protocol::GvspCodec::trailer(0, 0x1234, 2);
    REQUIRE(trailer.size() == 16);
    REQUIRE(trailer[4] == b(0x02));
    REQUIRE(trailer[11] == b(0x01));
    REQUIRE(trailer[12] == b(0x00));
    REQUIRE(trailer[13] == b(0x00));
    REQUIRE(trailer[14] == b(0x00));
    REQUIRE(trailer[15] == b(0x00));
}

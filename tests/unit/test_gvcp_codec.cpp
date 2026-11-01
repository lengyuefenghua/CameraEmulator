#include "protocol/gvcp_codec.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>

TEST_CASE("truncated GVCP command is rejected", "[GvcpCodec]") {
    constexpr std::array packet{
        std::byte{0x42}, std::byte{0x01}, std::byte{0x00}, std::byte{0x02},
        std::byte{0x00}, std::byte{0x08}, std::byte{0x00}, std::byte{0x01}
    };

    REQUIRE_FALSE(camera::protocol::GvcpCodec::decode(packet).has_value());
}

#include "protocol/gvcp_codec.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace {

std::byte b(unsigned int value) { return static_cast<std::byte>(value); }

} // namespace

TEST_CASE("Discovery ACK preserves request ID and exact 248-byte payload", "[GvcpGolden]") {
    std::array<std::byte, 0xf8> payload{};
    payload[0] = b(0x12);
    payload[1] = b(0x34);
    payload[0xf7] = b(0xab);
    const auto encoded = camera::protocol::GvcpCodec::discovery_ack(0x1234, payload);

    REQUIRE(encoded.size() == 0x100);
    REQUIRE(encoded[0] == b(0x00));
    REQUIRE(encoded[1] == b(0x00));
    REQUIRE(encoded[2] == b(0x00));
    REQUIRE(encoded[3] == b(0x03));
    REQUIRE(encoded[4] == b(0x00));
    REQUIRE(encoded[5] == b(0xf8));
    REQUIRE(encoded[6] == b(0x12));
    REQUIRE(encoded[7] == b(0x34));
    REQUIRE(encoded[8] == b(0x12));
    REQUIRE(encoded[9] == b(0x34));
    REQUIRE(encoded[0xff] == b(0xab));
}

TEST_CASE("Register ACK encoders match standard byte vectors", "[GvcpGolden]") {
    const std::array<std::byte, 4> value{b(0x12), b(0x34), b(0x56), b(0x78)};
    const auto read = camera::protocol::GvcpCodec::ack(0x0081, 0x0042, value);
    const std::vector<std::byte> readGolden{
        b(0x00), b(0x00), b(0x00), b(0x81), b(0x00), b(0x04), b(0x00), b(0x42),
        b(0x12), b(0x34), b(0x56), b(0x78)};
    REQUIRE(read == readGolden);

    const std::array<std::byte, 4> dataIndex{b(0x00), b(0x00), b(0x00), b(0x01)};
    const auto write = camera::protocol::GvcpCodec::ack(0x0083, 0x01fe, dataIndex);
    const std::vector<std::byte> writeGolden{
        b(0x00), b(0x00), b(0x00), b(0x83), b(0x00), b(0x04), b(0x01), b(0xfe),
        b(0x00), b(0x00), b(0x00), b(0x01)};
    REQUIRE(write == writeGolden);
}

#include "protocol/ipv4_udp_packet.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
std::byte b(unsigned value) { return static_cast<std::byte>(value); }
}

TEST_CASE("IPv4 UDP packet codec preserves endpoints and payload", "[Ipv4UdpPacket]") {
    const std::array<std::byte, 31> packet{
        b(0x45), b(0x00), b(0x00), b(0x1f), b(0x12), b(0x34), b(0x00), b(0x00),
        b(0x40), b(0x11), b(0x3a), b(0x7c), b(0xa9), b(0xfe), b(0x6d), b(0x10),
        b(0xa9), b(0xfe), b(0x6d), b(0x11), b(0x0f), b(0x74), b(0x0f), b(0x75),
        b(0x00), b(0x0b), b(0x00), b(0x00), b(0x47), b(0x56), b(0x43)};

    const auto decoded = camera::protocol::Ipv4UdpPacketCodec::decode(packet);

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->sourceAddress == 0xa9fe6d10u);
    REQUIRE(decoded->destinationAddress == 0xa9fe6d11u);
    REQUIRE(decoded->sourcePort == 3956);
    REQUIRE(decoded->destinationPort == 3957);
    REQUIRE(decoded->payload == std::vector<std::byte>{b(0x47), b(0x56), b(0x43)});
    const std::array<std::byte, 31> expected{
        b(0x45), b(0x00), b(0x00), b(0x1f), b(0x00), b(0x00), b(0x00), b(0x00),
        b(0x40), b(0x11), b(0x4c), b(0xb0), b(0xa9), b(0xfe), b(0x6d), b(0x10),
        b(0xa9), b(0xfe), b(0x6d), b(0x11), b(0x0f), b(0x74), b(0x0f), b(0x75),
        b(0x00), b(0x0b), b(0x00), b(0x00), b(0x47), b(0x56), b(0x43)};
    REQUIRE(camera::protocol::Ipv4UdpPacketCodec::encode(*decoded) == std::vector<std::byte>(expected.begin(), expected.end()));
}

TEST_CASE("IPv4 UDP packet codec rejects a corrupted header checksum", "[Ipv4UdpPacket]") {
    std::array<std::byte, 31> packet{
        b(0x45), b(0x00), b(0x00), b(0x1f), b(0x12), b(0x34), b(0x00), b(0x00),
        b(0x40), b(0x11), b(0x3a), b(0x7d), b(0xa9), b(0xfe), b(0x6d), b(0x10),
        b(0xa9), b(0xfe), b(0x6d), b(0x11), b(0x0f), b(0x74), b(0x0f), b(0x75),
        b(0x00), b(0x0b), b(0x00), b(0x00), b(0x47), b(0x56), b(0x43)};

    REQUIRE_FALSE(camera::protocol::Ipv4UdpPacketCodec::decode(packet).has_value());
}

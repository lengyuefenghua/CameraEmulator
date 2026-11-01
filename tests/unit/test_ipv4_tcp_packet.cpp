#include "protocol/ipv4_tcp_packet.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
std::byte b(unsigned value) { return static_cast<std::byte>(value); }
}

TEST_CASE("IPv4 TCP packet codec preserves SYN endpoint fields", "[Ipv4TcpPacket]") {
    const camera::protocol::Ipv4TcpSegment segment{0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000, 100, 0, 0x02, {}};
    const auto packet = camera::protocol::Ipv4TcpPacketCodec::encode(segment);
    const auto decoded = camera::protocol::Ipv4TcpPacketCodec::decode(packet);

    REQUIRE(packet.size() == 40);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->sourceAddress == segment.sourceAddress);
    REQUIRE(decoded->destinationAddress == segment.destinationAddress);
    REQUIRE(decoded->sourcePort == segment.sourcePort);
    REQUIRE(decoded->destinationPort == segment.destinationPort);
    REQUIRE(decoded->sequence == segment.sequence);
    REQUIRE(decoded->flags == segment.flags);
}

TEST_CASE("IPv4 TCP packet codec rejects a corrupted TCP checksum", "[Ipv4TcpPacket]") {
    const camera::protocol::Ipv4TcpSegment segment{0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000, 100, 0, 0x02, {}};
    auto packet = camera::protocol::Ipv4TcpPacketCodec::encode(segment);
    packet[37] = b(1);

    REQUIRE_FALSE(camera::protocol::Ipv4TcpPacketCodec::decode(packet).has_value());
}

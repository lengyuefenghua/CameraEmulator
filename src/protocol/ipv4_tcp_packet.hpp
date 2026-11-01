#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace camera::protocol {

struct Ipv4TcpSegment {
    std::uint32_t sourceAddress{};
    std::uint32_t destinationAddress{};
    std::uint16_t sourcePort{};
    std::uint16_t destinationPort{};
    std::uint32_t sequence{};
    std::uint32_t acknowledgement{};
    std::uint8_t flags{};
    std::vector<std::byte> payload;
};

class Ipv4TcpPacketCodec final {
public:
    [[nodiscard]] static std::optional<Ipv4TcpSegment> decode(std::span<const std::byte> packet);
    [[nodiscard]] static std::vector<std::byte> encode(const Ipv4TcpSegment& segment);
};

} // namespace camera::protocol

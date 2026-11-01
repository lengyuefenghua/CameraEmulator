#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace camera::protocol {

struct Ipv4UdpDatagram {
    std::uint32_t sourceAddress{};
    std::uint32_t destinationAddress{};
    std::uint16_t sourcePort{};
    std::uint16_t destinationPort{};
    std::vector<std::byte> payload;
};

class Ipv4UdpPacketCodec final {
public:
    [[nodiscard]] static std::optional<Ipv4UdpDatagram> decode(std::span<const std::byte> packet);
    [[nodiscard]] static std::vector<std::byte> encode(const Ipv4UdpDatagram& datagram);
};

} // namespace camera::protocol

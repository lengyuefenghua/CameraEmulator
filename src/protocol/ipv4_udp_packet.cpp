#include "protocol/ipv4_udp_packet.hpp"

#include <algorithm>
#include <limits>

namespace camera::protocol {
namespace {

std::uint16_t read_be16(std::span<const std::byte> bytes, std::size_t offset) {
    return (static_cast<std::uint16_t>(bytes[offset]) << 8) | static_cast<std::uint16_t>(bytes[offset + 1]);
}

std::uint32_t read_be32(std::span<const std::byte> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

void write_be16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value >> 8);
    bytes[offset + 1] = static_cast<std::byte>(value);
}

void write_be32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value >> 24);
    bytes[offset + 1] = static_cast<std::byte>(value >> 16);
    bytes[offset + 2] = static_cast<std::byte>(value >> 8);
    bytes[offset + 3] = static_cast<std::byte>(value);
}

std::uint16_t checksum(std::span<const std::byte> bytes) {
    std::uint32_t sum{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
        const auto high = static_cast<std::uint16_t>(bytes[offset]) << 8;
        const auto low = offset + 1 < bytes.size() ? static_cast<std::uint16_t>(bytes[offset + 1]) : 0;
        sum += high | low;
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return static_cast<std::uint16_t>(~sum);
}

} // namespace

std::optional<Ipv4UdpDatagram> Ipv4UdpPacketCodec::decode(std::span<const std::byte> packet) {
    if (packet.size() < 28 || static_cast<std::uint8_t>(packet[0]) >> 4 != 4) return std::nullopt;
    const auto headerLength = static_cast<std::size_t>(static_cast<std::uint8_t>(packet[0]) & 0x0f) * 4;
    if (headerLength != 20 || packet.size() < headerLength + 8 || packet[9] != std::byte{17}) return std::nullopt;
    const auto totalLength = read_be16(packet, 2);
    const auto fragmentFlags = read_be16(packet, 6);
    if (totalLength != packet.size() || fragmentFlags != 0 || checksum(packet.first(headerLength)) != 0) return std::nullopt;
    const auto udpLength = read_be16(packet, headerLength + 4);
    if (udpLength < 8 || udpLength != totalLength - headerLength) return std::nullopt;
    Ipv4UdpDatagram result;
    result.sourceAddress = read_be32(packet, 12);
    result.destinationAddress = read_be32(packet, 16);
    result.sourcePort = read_be16(packet, headerLength);
    result.destinationPort = read_be16(packet, headerLength + 2);
    result.payload.assign(packet.begin() + static_cast<std::ptrdiff_t>(headerLength + 8), packet.end());
    return result;
}

std::vector<std::byte> Ipv4UdpPacketCodec::encode(const Ipv4UdpDatagram& datagram) {
    const auto udpLength = datagram.payload.size() + 8;
    if (udpLength > std::numeric_limits<std::uint16_t>::max() - 20) return {};
    std::vector<std::byte> packet(20 + udpLength);
    packet[0] = std::byte{0x45};
    packet[8] = std::byte{64};
    packet[9] = std::byte{17};
    write_be16(packet, 2, static_cast<std::uint16_t>(packet.size()));
    write_be32(packet, 12, datagram.sourceAddress);
    write_be32(packet, 16, datagram.destinationAddress);
    write_be16(packet, 20, datagram.sourcePort);
    write_be16(packet, 22, datagram.destinationPort);
    write_be16(packet, 24, static_cast<std::uint16_t>(udpLength));
    std::copy(datagram.payload.begin(), datagram.payload.end(), packet.begin() + 28);
    write_be16(packet, 10, checksum(std::span<const std::byte>(packet.data(), 20)));
    return packet;
}

} // namespace camera::protocol

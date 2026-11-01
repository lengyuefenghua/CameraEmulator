#include "protocol/ipv4_tcp_packet.hpp"

#include <algorithm>
#include <limits>

namespace camera::protocol {
namespace {

std::uint16_t read16(std::span<const std::byte> bytes, std::size_t offset) {
    return (static_cast<std::uint16_t>(bytes[offset]) << 8) | static_cast<std::uint16_t>(bytes[offset + 1]);
}
std::uint32_t read32(std::span<const std::byte> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) | static_cast<std::uint32_t>(bytes[offset + 3]);
}
void write16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value >> 8);
    bytes[offset + 1] = static_cast<std::byte>(value);
}
void write32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value >> 24);
    bytes[offset + 1] = static_cast<std::byte>(value >> 16);
    bytes[offset + 2] = static_cast<std::byte>(value >> 8);
    bytes[offset + 3] = static_cast<std::byte>(value);
}
std::uint16_t checksum(std::span<const std::byte> bytes) {
    std::uint32_t sum{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += 2) {
        sum += (static_cast<std::uint16_t>(bytes[offset]) << 8) |
               (offset + 1 < bytes.size() ? static_cast<std::uint16_t>(bytes[offset + 1]) : 0);
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return static_cast<std::uint16_t>(~sum);
}
std::uint16_t tcp_checksum(std::uint32_t source, std::uint32_t destination, std::span<const std::byte> tcp) {
    std::vector<std::byte> pseudo(12 + tcp.size());
    write32(pseudo, 0, source);
    write32(pseudo, 4, destination);
    pseudo[9] = std::byte{6};
    write16(pseudo, 10, static_cast<std::uint16_t>(tcp.size()));
    std::copy(tcp.begin(), tcp.end(), pseudo.begin() + 12);
    return checksum(pseudo);
}
} // namespace

std::optional<Ipv4TcpSegment> Ipv4TcpPacketCodec::decode(std::span<const std::byte> packet) {
    // 隧道只支持无选项、未分片的 IPv4；分片无法在无重组层的相机端安全处理。
    if (packet.size() < 40 || static_cast<std::uint8_t>(packet[0]) != 0x45 || packet[9] != std::byte{6} ||
        (read16(packet, 6) & 0x3fffu) != 0 || read16(packet, 2) != packet.size() || checksum(packet.first(20)) != 0) return std::nullopt;
    if ((static_cast<std::uint8_t>(packet[32]) >> 4) * 4 != 20 ||
        static_cast<std::uint8_t>(packet[33]) & 0xc0u ||
        tcp_checksum(read32(packet, 12), read32(packet, 16), packet.subspan(20)) != 0) return std::nullopt;
    Ipv4TcpSegment result;
    result.sourceAddress = read32(packet, 12);
    result.destinationAddress = read32(packet, 16);
    result.sourcePort = read16(packet, 20);
    result.destinationPort = read16(packet, 22);
    result.sequence = read32(packet, 24);
    result.acknowledgement = read32(packet, 28);
    result.flags = static_cast<std::uint8_t>(packet[33]);
    result.payload.assign(packet.begin() + 40, packet.end());
    return result;
}

std::vector<std::byte> Ipv4TcpPacketCodec::encode(const Ipv4TcpSegment& segment) {
    if (segment.payload.size() > std::numeric_limits<std::uint16_t>::max() - 40) return {};
    std::vector<std::byte> packet(40 + segment.payload.size());
    packet[0] = std::byte{0x45};
    packet[8] = std::byte{64};
    packet[9] = std::byte{6};
    write16(packet, 2, static_cast<std::uint16_t>(packet.size()));
    write32(packet, 12, segment.sourceAddress);
    write32(packet, 16, segment.destinationAddress);
    write16(packet, 20, segment.sourcePort);
    write16(packet, 22, segment.destinationPort);
    write32(packet, 24, segment.sequence);
    write32(packet, 28, segment.acknowledgement);
    packet[32] = std::byte{0x50};
    packet[33] = static_cast<std::byte>(segment.flags);
    write16(packet, 34, 65535);
    std::copy(segment.payload.begin(), segment.payload.end(), packet.begin() + 40);
    write16(packet, 36, tcp_checksum(segment.sourceAddress, segment.destinationAddress, std::span<const std::byte>(packet.data() + 20, packet.size() - 20)));
    write16(packet, 10, checksum(std::span<const std::byte>(packet.data(), 20)));
    return packet;
}

} // namespace camera::protocol

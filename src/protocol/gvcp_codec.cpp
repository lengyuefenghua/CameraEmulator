#include "protocol/gvcp_codec.hpp"

#include <algorithm>

namespace camera::protocol {
namespace {

std::uint16_t read_be16(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(data[offset]) << 8) |
        std::to_integer<std::uint16_t>(data[offset + 1]));
}

void append_be16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>((value >> 8) & 0xff));
    out.push_back(static_cast<std::byte>(value & 0xff));
}

} // namespace

std::optional<GvcpCommand> GvcpCodec::decode(std::span<const std::byte> datagram) {
    constexpr std::size_t headerSize = 8;
    if (datagram.size() < headerSize || datagram[0] != std::byte{0x42}) return std::nullopt;
    const auto payloadSize = read_be16(datagram, 4);
    if (datagram.size() != headerSize + payloadSize) return std::nullopt;
    GvcpCommand command;
    command.type = GvcpPacketType::Command;
    command.flags = std::to_integer<std::uint8_t>(datagram[1]);
    command.opcode = read_be16(datagram, 2);
    command.declaredLength = payloadSize;
    command.requestId = read_be16(datagram, 6);
    command.payload.assign(datagram.begin() + headerSize, datagram.end());
    return command;
}

std::vector<std::byte> GvcpCodec::ack(std::uint16_t opcode, std::uint16_t requestId,
                                      std::span<const std::byte> payload) {
    std::vector<std::byte> result;
    result.reserve(8 + payload.size());
    result.push_back(std::byte{0x00});
    result.push_back(std::byte{0x00});
    append_be16(result, opcode);
    append_be16(result, static_cast<std::uint16_t>(payload.size()));
    append_be16(result, requestId);
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::vector<std::byte> GvcpCodec::error(std::uint16_t opcode, std::uint16_t requestId,
                                        std::uint16_t status) {
    std::vector<std::byte> result;
    result.reserve(8);
    result.push_back(std::byte{0x80});
    result.push_back(static_cast<std::byte>(status & 0xff));
    append_be16(result, opcode);
    append_be16(result, 0);
    append_be16(result, requestId);
    return result;
}

std::vector<std::byte> GvcpCodec::discovery_ack(
    std::uint16_t requestId, const std::array<std::byte, 0xf8>& data) {
    return ack(static_cast<std::uint16_t>(GvcpOpcode::DiscoveryAck), requestId, data);
}

} // namespace camera::protocol

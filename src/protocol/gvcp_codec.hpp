#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace camera::protocol {

enum class GvcpPacketType : std::uint8_t { Ack = 0x00, Command = 0x42, Error = 0x80 };

enum class GvcpOpcode : std::uint16_t {
    DiscoveryCommand = 0x0002,
    DiscoveryAck = 0x0003,
    ReadRegisterCommand = 0x0080,
    ReadRegisterAck = 0x0081,
    WriteRegisterCommand = 0x0082,
    WriteRegisterAck = 0x0083,
    ReadMemoryCommand = 0x0084,
    ReadMemoryAck = 0x0085,
    WriteMemoryCommand = 0x0086,
    WriteMemoryAck = 0x0087,
    EventCommand = 0x00c0,
    ActionCommand = 0x0100
};

struct GvcpCommand {
    GvcpPacketType type{};
    std::uint8_t flags{};
    std::uint16_t opcode{};
    std::uint16_t declaredLength{};
    std::uint16_t requestId{};
    std::vector<std::byte> payload;
};

class GvcpCodec final {
public:
    [[nodiscard]] static std::optional<GvcpCommand> decode(std::span<const std::byte> datagram);
    [[nodiscard]] static std::vector<std::byte> ack(std::uint16_t opcode, std::uint16_t requestId,
                                                     std::span<const std::byte> payload = {});
    [[nodiscard]] static std::vector<std::byte> error(std::uint16_t opcode, std::uint16_t requestId,
                                                      std::uint16_t status);
    [[nodiscard]] static std::vector<std::byte> discovery_ack(std::uint16_t requestId,
                                                               const std::array<std::byte, 0xf8>& data);
};

} // namespace camera::protocol

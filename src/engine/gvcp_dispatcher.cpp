#include "engine/gvcp_dispatcher.hpp"

#include "infra/logging.hpp"
#include "protocol/genicam_xml_generator.hpp"

using camera::infra::Logging;

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string_view>

namespace camera::engine {
namespace {
constexpr std::uint16_t kReadRegister = 0x0080;
constexpr std::uint16_t kReadRegisterAck = 0x0081;
constexpr std::uint16_t kWriteRegister = 0x0082;
constexpr std::uint16_t kWriteRegisterAck = 0x0083;
constexpr std::uint16_t kReadMemory = 0x0084;
constexpr std::uint16_t kReadMemoryAck = 0x0085;
constexpr std::uint16_t kWriteMemory = 0x0086;
constexpr std::uint16_t kWriteMemoryAck = 0x0087;
constexpr std::uint16_t kDiscovery = 0x0002;
constexpr std::uint16_t kNotImplemented = 1;
constexpr std::uint16_t kInvalidParameter = 2;
constexpr std::uint16_t kInvalidAccess = 3;
constexpr std::uint16_t kAccessDenied = 6;
constexpr std::uint32_t kXmlUrl0 = 0x200;
constexpr std::uint32_t kXmlUrlSize = 512;
constexpr std::uint32_t kXmlUrl1 = 0x400;
constexpr std::uint32_t kXmlData = 0x2000;

void put_be32(std::byte* out, std::uint32_t value) {
    out[0] = static_cast<std::byte>(value >> 24);
    out[1] = static_cast<std::byte>(value >> 16);
    out[2] = static_cast<std::byte>(value >> 8);
    out[3] = static_cast<std::byte>(value);
}

void put_text(std::array<std::byte, 0xf8>& out, std::size_t offset, std::size_t length, std::string_view text) {
    const auto count = std::min(length, text.size());
    std::memcpy(out.data() + offset, text.data(), count);
}

void log_dispatch(const char* level, const std::string& source, const std::string& message) {
    const auto parsedLevel = std::string_view(level) == "ERROR" ? spdlog::level::err
        : std::string_view(level) == "WARN" ? spdlog::level::warn : spdlog::level::info;
    Logging::write(parsedLevel, "GvcpDispatcher", source, message);
}

} // namespace

GvcpDispatcher::GvcpDispatcher(core::RegisterMap& registers, SessionController& session,
                               AcquisitionHandler acquisitionHandler)
    : registers_(registers), session_(session), acquisitionHandler_(std::move(acquisitionHandler)),
      xmlBootstrap_(protocol::GenicamXmlGenerator::generate(registers)) {
    std::ostringstream descriptor;
    descriptor << "Local:///" << registers_.config().id << ".xml;"
               << std::hex << kXmlData << ";" << xmlBootstrap_.size();
    xmlUrlDescriptor_ = descriptor.str();
}

std::uint32_t GvcpDispatcher::read_be32(const std::vector<std::byte>& bytes, std::size_t offset) {
    return (std::to_integer<std::uint32_t>(bytes[offset]) << 24) |
           (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 8) |
           std::to_integer<std::uint32_t>(bytes[offset + 3]);
}

std::vector<std::byte> GvcpDispatcher::u32_payload(std::uint32_t value) {
    std::vector<std::byte> result(4);
    put_be32(result.data(), value);
    return result;
}

std::array<std::byte, 0xf8> GvcpDispatcher::discovery_payload() {
    std::array<std::byte, 0xf8> payload{};
    const auto& config = registers_.config();
    (void)registers_.write_string(kXmlUrl0, xmlUrlDescriptor_);
    put_be32(payload.data() + 0x00, 0x00020002);
    put_be32(payload.data() + 0x04, 0x80000000);
    put_be32(payload.data() + 0x08, 0x00000200);
    put_be32(payload.data() + 0x0c, 0x00000001);
    put_be32(payload.data() + 0x10, 0x00000007);
    put_be32(payload.data() + 0x14, 0x00000001);
    put_be32(payload.data() + 0x24, config.ip.host_value());
    put_be32(payload.data() + 0x34, config.subnetMask.host_value());
    put_be32(payload.data() + 0x44, config.gateway.host_value());
    put_text(payload, 0x48, 32, config.vendor);
    put_text(payload, 0x68, 32, config.model);
    put_text(payload, 0x88, 32, config.version);
    put_text(payload, 0xa8, 48, config.manufacturerInfo);
    put_text(payload, 0xd8, 16, config.serial);
    put_text(payload, 0xe8, 16, config.id);
    return payload;
}

std::optional<std::vector<char>> GvcpDispatcher::dispatch(const infra::win32::Datagram& datagram) {
    const auto* data = reinterpret_cast<const std::byte*>(datagram.bytes.data());
    const auto decoded = protocol::GvcpCodec::decode(std::span<const std::byte>(data, datagram.bytes.size()));
    if (!decoded) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastMalformedLog_ >= std::chrono::seconds(1)) {
            log_dispatch("WARN", "malformed", "peer=" + datagram.sourceIp + " bytes=" + std::to_string(datagram.bytes.size()));
            lastMalformedLog_ = now;
        }
        return std::nullopt;
    }

    std::vector<std::byte> response;
    const auto& cmd = *decoded;
    if (cmd.opcode == kDiscovery && cmd.payload.empty()) {
        const auto payload = discovery_payload();
        response = protocol::GvcpCodec::discovery_ack(cmd.requestId, payload);
    } else if (cmd.opcode == kReadRegister && cmd.payload.size() == 4) {
        const auto address = read_be32(cmd.payload, 0);
        if (address == 0xa00) {
            const auto value = session_.owns(datagram.sourceIp)
                ? registers_.read_u32(address).value_or(0) : 0;
            if (value != 0) (void)session_.heartbeat(datagram.sourceIp);
            response = protocol::GvcpCodec::ack(kReadRegisterAck, cmd.requestId, u32_payload(value));
        } else if (address == 0x938) {
            (void)session_.heartbeat(datagram.sourceIp);
            response = protocol::GvcpCodec::ack(kReadRegisterAck, cmd.requestId,
                                                u32_payload(session_.read_register(address).value_or(0)));
        } else if (address >= kXmlUrl0 && address < kXmlUrl1 + kXmlUrlSize) {
            response = protocol::GvcpCodec::error(kReadRegisterAck, cmd.requestId, kInvalidAccess);
        } else {
            const auto value = session_.read_register(address);
            if (!value) {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastUnknownLog_ >= std::chrono::seconds(1)) {
                    log_dispatch("WARN", "unknown-register", "peer=" + datagram.sourceIp + " opcode=READREG address=" + std::to_string(address));
                    lastUnknownLog_ = now;
                }
            }
            response = protocol::GvcpCodec::ack(kReadRegisterAck, cmd.requestId, u32_payload(value.value_or(0)));
        }
    } else if (cmd.opcode == kWriteRegister && cmd.payload.size() == 8) {
        const auto address = read_be32(cmd.payload, 0);
        const auto value = read_be32(cmd.payload, 4);
        std::string error;
        if (!session_.write_register(datagram.sourceIp, address, value, error)) {
            const bool denied = address == 0xa00 || error.find("privilege") != std::string::npos || error.find("control") != std::string::npos;
            if (!denied && error.find("unsupported") != std::string::npos) {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastUnknownLog_ >= std::chrono::seconds(1)) {
                    log_dispatch("WARN", "unknown-register", "peer=" + datagram.sourceIp + " opcode=WRITEREG address=" + std::to_string(address));
                    lastUnknownLog_ = now;
                }
            }
            response = protocol::GvcpCodec::error(kWriteRegisterAck, cmd.requestId, denied ? kAccessDenied : kInvalidAccess);
        } else {
            if (address == 0x124 && acquisitionHandler_) {
                if (!acquisitionHandler_(value == 1, error)) {
                    log_dispatch("ERROR", "acquisition", "camera=" + registers_.config().id + " " + error);
                    response = protocol::GvcpCodec::error(kWriteRegisterAck, cmd.requestId, kInvalidAccess);
                } else {
                    response = protocol::GvcpCodec::ack(kWriteRegisterAck, cmd.requestId, u32_payload(1));
                }
            } else {
                response = protocol::GvcpCodec::ack(kWriteRegisterAck, cmd.requestId, u32_payload(1));
            }
        }
    } else if (cmd.opcode == kReadMemory && cmd.payload.size() == 8) {
        const auto address = read_be32(cmd.payload, 0);
        const auto length = read_be32(cmd.payload, 4);
        if (length == 0 || length > 1024 || (length & 3u) != 0 || address > 0x10000 || address + length > 0x10000) {
            response = protocol::GvcpCodec::error(kReadMemoryAck, cmd.requestId, kInvalidParameter);
        } else {
            std::vector<std::byte> payload(4 + length);
            put_be32(payload.data(), address);
            if (address >= kXmlUrl0 && address < kXmlUrl0 + kXmlUrlSize && address + length <= kXmlUrl0 + kXmlUrlSize) {
                for (std::uint32_t offset = 0; offset < length; ++offset) {
                    const auto index = address - kXmlUrl0 + offset;
                    payload[4 + offset] = index < xmlUrlDescriptor_.size()
                        ? static_cast<std::byte>(xmlUrlDescriptor_[index]) : std::byte{0};
                }
            } else if (address >= kXmlUrl1 && address < kXmlUrl1 + kXmlUrlSize && address + length <= kXmlUrl1 + kXmlUrlSize) {
                std::fill(payload.begin() + 4, payload.end(), std::byte{0});
            } else if (address >= kXmlData && address < kXmlData + xmlBootstrap_.size() &&
                       address + length <= kXmlData + xmlBootstrap_.size()) {
                for (std::uint32_t offset = 0; offset < length; ++offset) {
                    payload[4 + offset] = static_cast<std::byte>(xmlBootstrap_[address - kXmlData + offset]);
                }
            } else {
                const auto textRange = registers_.read_string_range(address, length);
                if (textRange) {
                    for (std::uint32_t offset = 0; offset < length; ++offset) {
                        payload[4 + offset] = offset < textRange->size()
                            ? static_cast<std::byte>((*textRange)[offset]) : std::byte{0};
                    }
                } else {
                    for (std::uint32_t offset = 0; offset + 4 <= length; offset += 4) {
                        put_be32(payload.data() + 4 + offset,
                                 session_.read_register(address + offset).value_or(0));
                    }
                }
            }
            response = protocol::GvcpCodec::ack(kReadMemoryAck, cmd.requestId, payload);
        }
    } else if (cmd.opcode == kWriteMemory && cmd.payload.size() >= 8) {
        const auto address = read_be32(cmd.payload, 0);
        const auto dataLength = cmd.payload.size() - 4;
        if (dataLength > 512 || address > 0x10000 || address + dataLength > 0x10000) {
            response = protocol::GvcpCodec::error(kWriteMemoryAck, cmd.requestId, kInvalidParameter);
        } else {
            response = protocol::GvcpCodec::error(kWriteMemoryAck, cmd.requestId, kAccessDenied);
        }
    } else {
        response = protocol::GvcpCodec::error(cmd.opcode, cmd.requestId, kNotImplemented);
    }

    std::vector<char> bytes(response.size());
    std::transform(response.begin(), response.end(), bytes.begin(), [](std::byte value) {
        return static_cast<char>(std::to_integer<unsigned char>(value));
    });
    return bytes;
}

} // namespace camera::engine

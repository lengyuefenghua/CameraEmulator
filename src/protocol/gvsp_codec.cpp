#include "protocol/gvsp_codec.hpp"

#include <algorithm>

namespace camera::protocol {
namespace {
constexpr std::uint32_t kContentLeader = 1;
constexpr std::uint32_t kContentTrailer = 2;
constexpr std::uint32_t kContentPayload = 3;
constexpr std::uint16_t kImagePayloadType = 1;

void put_be16(std::byte* out, std::uint16_t value) {
    out[0] = static_cast<std::byte>(value >> 8);
    out[1] = static_cast<std::byte>(value);
}
void put_be32(std::byte* out, std::uint32_t value) {
    out[0] = static_cast<std::byte>(value >> 24);
    out[1] = static_cast<std::byte>(value >> 16);
    out[2] = static_cast<std::byte>(value >> 8);
    out[3] = static_cast<std::byte>(value);
}
std::vector<std::byte> header(std::uint16_t block, std::uint32_t packetId, std::uint32_t content, std::size_t payloadSize) {
    std::vector<std::byte> result(8 + payloadSize);
    put_be16(result.data(), 0); // GVSP status
    put_be16(result.data() + 2, block);
    put_be32(result.data() + 4, (content << 24) | (packetId & 0x00ffffff));
    return result;
}
} // namespace

std::vector<std::byte> GvspCodec::leader(const GvspFrameInfo& info) {
    auto result = header(info.blockId, 0, kContentLeader, 36);
    put_be16(result.data() + 8, 0);
    put_be16(result.data() + 10, kImagePayloadType);
    put_be32(result.data() + 12, 0);
    put_be32(result.data() + 16, 0);
    put_be32(result.data() + 20, static_cast<std::uint32_t>(info.pixelFormat));
    put_be32(result.data() + 24, info.width);
    put_be32(result.data() + 28, info.height);
    put_be32(result.data() + 32, 0);
    put_be32(result.data() + 36, 0);
    put_be16(result.data() + 40, 0);
    put_be16(result.data() + 42, 0);
    return result;
}

std::vector<std::byte> GvspCodec::payload(std::uint16_t, std::uint16_t blockId, std::uint32_t packetId,
                                           std::span<const std::byte> data, bool last) {
    (void)last;
    auto result = header(blockId, packetId, kContentPayload, data.size());
    std::copy(data.begin(), data.end(), result.begin() + 8);
    return result;
}

std::vector<std::byte> GvspCodec::trailer(std::uint16_t, std::uint16_t blockId, std::uint32_t packetId) {
    auto result = header(blockId, packetId, kContentTrailer, 8);
    put_be32(result.data() + 8, kImagePayloadType);
    put_be32(result.data() + 12, 0);
    return result;
}

} // namespace camera::protocol

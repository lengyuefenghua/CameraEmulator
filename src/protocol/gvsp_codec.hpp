#pragma once

#include "core/camera_model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace camera::protocol {

struct GvspFrameInfo {
    std::uint16_t streamChannel{};
    std::uint16_t blockId{};
    std::uint32_t width{};
    std::uint32_t height{};
    core::PixelFormat pixelFormat{core::PixelFormat::Mono8};
};

class GvspCodec final {
public:
    [[nodiscard]] static std::vector<std::byte> leader(const GvspFrameInfo& info);
    [[nodiscard]] static std::vector<std::byte> payload(std::uint16_t streamChannel, std::uint16_t blockId,
                                                        std::uint32_t packetId, std::span<const std::byte> data,
                                                        bool last);
    [[nodiscard]] static std::vector<std::byte> trailer(std::uint16_t streamChannel, std::uint16_t blockId,
                                                        std::uint32_t packetId);
};

} // namespace camera::protocol

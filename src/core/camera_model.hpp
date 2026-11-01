#pragma once

#include <cstdint>
#include <string>

namespace camera::core {

enum class PixelFormat : std::uint32_t {
    Mono8 = 17301505,
    RGB8 = 35127316
};

struct Ipv4Address {
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint8_t c{};
    std::uint8_t d{};

    [[nodiscard]] std::uint32_t host_value() const noexcept;
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool is_loopback() const noexcept;
    static Ipv4Address parse(const std::string& text);
};

struct CameraConfig {
    std::string id{"cam1"};
    std::string vendor{"CameraEmulator"};
    std::string model{"VirtualGigE"};
    std::string serial{"CE000001"};
    std::string version{"1.0.0"};
    std::string manufacturerInfo{"Standard GigE Vision simulator"};
    Ipv4Address ip{127, 0, 0, 2};
    Ipv4Address subnetMask{255, 0, 0, 0};
    Ipv4Address gateway{0, 0, 0, 0};
    std::uint16_t httpPort{8000};
    std::uint16_t gvspPort{3957};
    std::uint32_t width{640};
    std::uint32_t height{480};
    PixelFormat pixelFormat{PixelFormat::Mono8};
    std::uint32_t packetSize{1400};
    std::uint32_t packetDelay{0};
    std::uint32_t heartbeatTimeoutMs{3000};
    std::uint32_t frameRate{10};
    std::string imagePath;
};

enum class RuntimeState { Stopped, Initializing, Running, Error };

struct RuntimeStatus {
    RuntimeState state{RuntimeState::Stopped};
    std::uint64_t frameCount{};
    std::uint64_t droppedFrames{};
    std::uint32_t currentFrameRate{};
    std::string lastError;
};

} // namespace camera::core

#include "core/camera_model.hpp"

#include <array>
#include <charconv>
#include <stdexcept>
#include <string_view>

namespace camera::core {

std::uint32_t Ipv4Address::host_value() const noexcept {
    return (static_cast<std::uint32_t>(a) << 24) |
           (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(c) << 8) |
           static_cast<std::uint32_t>(d);
}

std::string Ipv4Address::to_string() const {
    return std::to_string(a) + "." + std::to_string(b) + "." +
           std::to_string(c) + "." + std::to_string(d);
}

bool Ipv4Address::is_loopback() const noexcept { return a == 127; }

Ipv4Address Ipv4Address::parse(const std::string& text) {
    std::array<std::uint8_t, 4> octets{};
    std::size_t begin = 0;
    for (std::size_t i = 0; i < octets.size(); ++i) {
        const auto end = text.find('.', begin);
        const auto tokenEnd = end == std::string::npos ? text.size() : end;
        if ((i < 3 && end == std::string::npos) || (i == 3 && end != std::string::npos)) {
            throw std::invalid_argument("IPv4 地址必须由四个十进制段组成");
        }
        unsigned value{};
        const auto first = text.data() + begin;
        const auto last = text.data() + tokenEnd;
        const auto [ptr, ec] = std::from_chars(first, last, value);
        if (ec != std::errc{} || ptr != last || value > 255 || tokenEnd == begin) {
            throw std::invalid_argument("IPv4 地址段无效");
        }
        octets[i] = static_cast<std::uint8_t>(value);
        begin = tokenEnd + 1;
    }
    return {octets[0], octets[1], octets[2], octets[3]};
}

} // namespace camera::core

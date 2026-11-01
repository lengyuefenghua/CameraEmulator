#pragma once

#include "core/register_map.hpp"
#include "engine/session_controller.hpp"
#include "infra/win32/udp_pktinfo_transport.hpp"
#include "protocol/gvcp_codec.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace camera::engine {

class GvcpDispatcher final {
public:
    using AcquisitionHandler = std::function<bool(bool start, std::string& error)>;

    GvcpDispatcher(core::RegisterMap& registers, SessionController& session, AcquisitionHandler acquisitionHandler);
    [[nodiscard]] std::optional<std::vector<char>> dispatch(const infra::win32::Datagram& datagram);

private:
    core::RegisterMap& registers_;
    SessionController& session_;
    AcquisitionHandler acquisitionHandler_;
    std::chrono::steady_clock::time_point lastMalformedLog_{};
    std::chrono::steady_clock::time_point lastUnknownLog_{};
    std::string xmlBootstrap_;
    std::string xmlUrlDescriptor_;
    [[nodiscard]] std::array<std::byte, 0xf8> discovery_payload();
    [[nodiscard]] static std::uint32_t read_be32(const std::vector<std::byte>& bytes, std::size_t offset);
    [[nodiscard]] static std::vector<std::byte> u32_payload(std::uint32_t value);
};

} // namespace camera::engine

#pragma once

#include "core/register_map.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace camera::engine {

class SessionController final {
public:
    explicit SessionController(core::RegisterMap& registers);

    [[nodiscard]] bool acquire(std::string peerIp, std::uint32_t privilege, std::string& error);
    [[nodiscard]] bool release(const std::string& peerIp);
    [[nodiscard]] bool heartbeat(const std::string& peerIp);
    [[nodiscard]] bool owns(const std::string& peerIp);
    [[nodiscard]] bool write_register(const std::string& peerIp, std::uint32_t address,
                                      std::uint32_t value, std::string& error);
    [[nodiscard]] std::optional<std::uint32_t> read_register(std::uint32_t address) const;
    void expire_if_idle();
    [[nodiscard]] std::string owner() const { return ownerIp_; }
    [[nodiscard]] bool has_owner() { return has_live_owner(); }

private:
    core::RegisterMap& registers_;
    std::string ownerIp_;
    std::chrono::steady_clock::time_point lastHeartbeat_{};

    [[nodiscard]] bool has_live_owner();
};

} // namespace camera::engine

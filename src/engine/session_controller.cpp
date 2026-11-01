#include "engine/session_controller.hpp"

#include "infra/logging.hpp"

using camera::infra::Logging;

#include <chrono>
#include <string_view>

namespace camera::engine {
namespace {
constexpr std::uint32_t kCcpAddress = 0xa00;
constexpr std::uint32_t kHeartbeatAddress = 0x938;
constexpr std::uint32_t kExclusivePrivilege = 1;
constexpr std::uint32_t kControlPrivilege = 2;

void log_session(const char* level, const std::string& source, const std::string& message) {
    const auto parsedLevel = std::string_view(level) == "ERROR" ? spdlog::level::err
        : std::string_view(level) == "WARN" ? spdlog::level::warn : spdlog::level::info;
    Logging::write(parsedLevel, "SessionController", source, message);
}
}

SessionController::SessionController(core::RegisterMap& registers) : registers_(registers) {}

bool SessionController::has_live_owner() {
    if (ownerIp_.empty()) return false;
    const auto timeout = registers_.read_u32(kHeartbeatAddress).value_or(3000);
    if (std::chrono::steady_clock::now() - lastHeartbeat_ <= std::chrono::milliseconds(timeout)) return true;
    const auto expiredOwner = ownerIp_;
    ownerIp_.clear();
    (void)registers_.write_u32(kCcpAddress, 0);
    log_session("WARN", "heartbeat-expired", "peer=" + expiredOwner);
    return false;
}

bool SessionController::acquire(std::string peerIp, std::uint32_t privilege, std::string& error) {
    const auto allowedBits = kExclusivePrivilege | kControlPrivilege;
    if ((privilege & ~allowedBits) != 0) {
        error = "unsupported CCP privilege bits";
        return false;
    }
    if (privilege == 0) return release(peerIp);
    if ((privilege & kControlPrivilege) == 0) {
        error = "CCP privilege does not request control access";
        return false;
    }
    if (has_live_owner() && ownerIp_ != peerIp) {
        error = "control privilege is held by another client";
        return false;
    }
    ownerIp_ = std::move(peerIp);
    lastHeartbeat_ = std::chrono::steady_clock::now();
    (void)registers_.write_u32(kCcpAddress, privilege);
    log_session("INFO", "ccp-acquired", "peer=" + ownerIp_ + " privilege=" + std::to_string(privilege));
    return true;
}

bool SessionController::release(const std::string& peerIp) {
    if (!ownerIp_.empty() && ownerIp_ != peerIp) return false;
    const auto releasedOwner = ownerIp_;
    ownerIp_.clear();
    (void)registers_.write_u32(kCcpAddress, 0);
    if (!releasedOwner.empty()) log_session("INFO", "ccp-released", "peer=" + releasedOwner);
    return true;
}

bool SessionController::heartbeat(const std::string& peerIp) {
    if (!has_live_owner() || ownerIp_ != peerIp) return false;
    lastHeartbeat_ = std::chrono::steady_clock::now();
    return true;
}

bool SessionController::owns(const std::string& peerIp) {
    return has_live_owner() && ownerIp_ == peerIp;
}

bool SessionController::write_register(const std::string& peerIp, std::uint32_t address,
                                       std::uint32_t value, std::string& error) {
    if (address == kCcpAddress) return acquire(peerIp, value & 0x3, error);
    if (address == kHeartbeatAddress) {
        if (!owns(peerIp)) {
            error = "heartbeat write without control privilege";
            return false;
        }
        lastHeartbeat_ = std::chrono::steady_clock::now();
        return registers_.write_u32(address, value);
    }
    if (!owns(peerIp)) {
        error = "register write without control privilege";
        return false;
    }
    if (!registers_.write_u32(address, value)) {
        error = "register write rejected or address unsupported";
        return false;
    }
    lastHeartbeat_ = std::chrono::steady_clock::now();
    return true;
}

std::optional<std::uint32_t> SessionController::read_register(std::uint32_t address) const {
    return registers_.read_u32(address);
}

void SessionController::expire_if_idle() { (void)has_live_owner(); }

} // namespace camera::engine

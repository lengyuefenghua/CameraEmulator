#pragma once

#ifdef _WIN32

#include "core/camera_model.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace camera::infra::win32 {

struct WintunPeerInfo {
    std::string adapterName;
    core::Ipv4Address hostAddress;
    core::Ipv4Address cameraAddress;
};

class WintunPeer final {
public:
    WintunPeer() = default;
    ~WintunPeer();
    WintunPeer(const WintunPeer&) = delete;
    WintunPeer& operator=(const WintunPeer&) = delete;

    [[nodiscard]] bool start(const core::CameraConfig& config, const std::filesystem::path& applicationDirectory,
                             std::string& error);
    [[nodiscard]] std::optional<std::vector<std::byte>> receive(std::string& error);
    [[nodiscard]] bool send(std::span<const std::byte> packet, std::string& error);
    void stop() noexcept;
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] const WintunPeerInfo& info() const noexcept { return info_; }

private:
    struct Implementation;
    mutable std::mutex implementationMutex_;
    std::shared_ptr<Implementation> implementation_;
    WintunPeerInfo info_;
};

} // namespace camera::infra::win32

#endif

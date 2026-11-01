#pragma once

#include "core/camera_model.hpp"
#include "engine/camera_instance.hpp"

#include <memory>
#include <string>
#include <vector>

namespace camera::engine {

struct PeerRuntimeStatus {
    bool running{};
    std::string adapterName;
    core::Ipv4Address hostAddress{};
    core::Ipv4Address cameraAddress{};
    std::string error;
};

class EngineFacade final {
public:
    EngineFacade() = default;
    ~EngineFacade();
    EngineFacade(const EngineFacade&) = delete;
    EngineFacade& operator=(const EngineFacade&) = delete;

    [[nodiscard]] bool create_camera(core::CameraConfig config, std::string& error);
    [[nodiscard]] bool start(std::string& error);
    void stop() noexcept;
    [[nodiscard]] core::RuntimeStatus status() const;
    [[nodiscard]] PeerRuntimeStatus peer_status() const;
    [[nodiscard]] std::string xml_preview() const;
    [[nodiscard]] std::vector<core::FeatureSpec> features() const;
    [[nodiscard]] core::RegisterMap* registers() noexcept;
    [[nodiscard]] core::CameraConfig config() const;
    [[nodiscard]] bool update_config(core::CameraConfig config, std::string& error);
    [[nodiscard]] bool switch_and_start(core::CameraConfig config, std::string& error);
    [[nodiscard]] bool write_register(const std::string& peerIp, std::uint32_t address,
                                      std::uint32_t value, std::string& error);
    [[nodiscard]] bool acquire_control(const std::string& peerIp, std::string& error);
    [[nodiscard]] bool acquire_control(std::string peerIp, std::uint32_t privilege, std::string& error);
    [[nodiscard]] bool start_acquisition(std::string& error);
    void stop_acquisition() noexcept;

private:
    std::unique_ptr<CameraInstance> camera_;
    std::string peerError_;
};

} // namespace camera::engine

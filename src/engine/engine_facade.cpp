#include "engine/engine_facade.hpp"

#include "core/validation.hpp"
#include "infra/logging.hpp"
#include "protocol/genicam_xml_generator.hpp"

using camera::infra::Logging;

namespace camera::engine {
namespace {
void log_error(const std::string& module, const std::string& source, const std::string& message) {
    Logging::write(spdlog::level::err, module, source, message);
}
}

EngineFacade::~EngineFacade() { stop(); }

bool EngineFacade::create_camera(core::CameraConfig config, std::string& error) {
    const auto validation = core::validate(config);
    if (!validation.valid()) {
        error = validation.errors.front();
        return false;
    }
    if (camera_) {
        error = "single-camera instance already exists";
        return false;
    }
    camera_ = std::make_unique<CameraInstance>(std::move(config));
    return true;
}

bool EngineFacade::start(std::string& error) {
    if (!camera_) {
        error = "camera has not been configured";
        peerError_ = error;
        return false;
    }
    if (!camera_->start(error)) {
        peerError_ = error;
        log_error("EngineFacade", "peer-start", "camera=" + camera_->config().id + " " + error);
        return false;
    }
    peerError_.clear();
    return true;
}

void EngineFacade::stop() noexcept {
    if (camera_) camera_->stop();
    peerError_.clear();
}

bool EngineFacade::acquire_control(std::string peerIp, std::uint32_t privilege, std::string& error) {
    if (!camera_) {
        error = "camera has not been configured";
        return false;
    }
    return camera_->session().acquire(std::move(peerIp), privilege, error);
}

bool EngineFacade::start_acquisition(std::string& error) {
    if (!camera_ || !camera_->registers().acquisition_running()) {
        error = "acquisition start command has not been written";
        return false;
    }
    return camera_->stream().start(error);
}

void EngineFacade::stop_acquisition() noexcept {
    if (camera_) camera_->stream().stop();
}

core::RuntimeStatus EngineFacade::status() const {
    return camera_ ? camera_->stream().status() : core::RuntimeStatus{};
}

PeerRuntimeStatus EngineFacade::peer_status() const {
    PeerRuntimeStatus result;
    result.error = peerError_;
    if (!camera_ || !camera_->peer_running()) return result;
    const auto& peer = camera_->peer_info();
    result.running = true;
    result.adapterName = peer.adapterName;
    result.hostAddress = peer.hostAddress;
    result.cameraAddress = peer.cameraAddress;
    return result;
}

std::string EngineFacade::xml_preview() const {
    if (!camera_) return {};
    return protocol::GenicamXmlGenerator::generate(camera_->registers());
}

std::vector<core::FeatureSpec> EngineFacade::features() const {
    if (!camera_) return {};
    return camera_->registers().features();
}

core::RegisterMap* EngineFacade::registers() noexcept {
    return camera_ ? &camera_->registers() : nullptr;
}

core::CameraConfig EngineFacade::config() const {
    return camera_ ? camera_->config() : core::CameraConfig{};
}

bool EngineFacade::update_config(core::CameraConfig config, std::string& error) {
    if (camera_) {
        camera_->stop();
        camera_.reset();
    }
    return create_camera(std::move(config), error);
}

bool EngineFacade::switch_and_start(core::CameraConfig config, std::string& error) {
    const auto nextId = config.id;
    const auto previousId = camera_ ? camera_->config().id : std::string{"none"};
    if (!update_config(std::move(config), error)) {
        log_error("EngineFacade", "profile-switch", "previous=" + previousId + " next=" + nextId + " reason=" + error);
        return false;
    }
    if (start(error)) {
        Logging::write(spdlog::level::info, "EngineFacade", "profile-switch",
                       "previous=" + previousId + " next=" + nextId);
        return true;
    }
    if (camera_) {
        camera_->stop();
        camera_.reset();
    }
    log_error("EngineFacade", "profile-switch", "previous=" + previousId + " next=" + nextId + " reason=" + error);
    return false;
}

bool EngineFacade::acquire_control(const std::string& peerIp, std::string& error) {
    if (!camera_) {
        error = "camera has not been configured";
        return false;
    }
    return camera_->session().acquire(peerIp, 2, error);
}

bool EngineFacade::write_register(const std::string& peerIp, std::uint32_t address,
                                  std::uint32_t value, std::string& error) {
    if (!camera_ || !camera_->session().owns(peerIp)) {
        error = "client does not own the camera control channel";
        return false;
    }
    const bool started = camera_->registers().acquisition_running();
    if (!camera_->session().write_register(peerIp, address, value, error)) return false;
    const bool nowRunning = camera_->registers().acquisition_running();
    if (!started && nowRunning && !camera_->stream().start(error)) {
        // 预载/流启动失败时撤销采集状态，避免寄存器与实际流状态分离。
        log_error("EngineFacade", "acquisition-start", "camera=" + camera_->config().id + " reason=" + error);
        (void)camera_->session().release(peerIp);
        return false;
    }
    if (started && !nowRunning) camera_->stream().stop();
    return true;
}

} // namespace camera::engine

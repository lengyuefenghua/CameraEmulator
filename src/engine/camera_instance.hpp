#pragma once

#include "core/camera_model.hpp"
#include "core/register_map.hpp"
#include "engine/gvcp_dispatcher.hpp"
#include "engine/session_controller.hpp"
#include "engine/stream_controller.hpp"
#include "infra/tunnel_http_server.hpp"
#include "infra/win32/wintun_peer.hpp"

#include <atomic>
#include <string>
#include <thread>

namespace camera::engine {

class CameraInstance final {
public:
    explicit CameraInstance(core::CameraConfig config);
    ~CameraInstance();
    CameraInstance(const CameraInstance&) = delete;
    CameraInstance& operator=(const CameraInstance&) = delete;

    [[nodiscard]] bool start(std::string& error);
    void stop() noexcept;
    [[nodiscard]] const core::CameraConfig& config() const noexcept { return registers_.config(); }
    [[nodiscard]] core::RegisterMap& registers() noexcept { return registers_; }
    [[nodiscard]] SessionController& session() noexcept { return session_; }
    [[nodiscard]] StreamController& stream() noexcept { return stream_; }
    [[nodiscard]] bool peer_running() const noexcept { return peer_.is_running(); }
    [[nodiscard]] const infra::win32::WintunPeerInfo& peer_info() const noexcept { return peer_.info(); }

private:
    core::RegisterMap registers_;
    SessionController session_;
    GvcpDispatcher dispatcher_;
    infra::win32::WintunPeer peer_;
    infra::TunnelHttpServer xmlServer_;
    StreamController stream_;
    std::atomic<bool> running_{};
    std::thread receiveThread_;
    void receive_loop();
};

} // namespace camera::engine

#pragma once

#include "core/camera_model.hpp"
#include "core/register_map.hpp"
#include "engine/frame_pool.hpp"
#include "infra/win32/wintun_peer.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace camera::engine {

class StreamController final {
public:
    StreamController(core::RegisterMap& registers, infra::win32::WintunPeer& peer);
    ~StreamController();
    StreamController(const StreamController&) = delete;
    StreamController& operator=(const StreamController&) = delete;

    [[nodiscard]] bool start(std::string& error);
    void stop() noexcept;
    [[nodiscard]] core::RuntimeStatus status() const;

private:
    core::RegisterMap& registers_;
    infra::win32::WintunPeer& peer_;
    FramePool pool_{3};
    std::vector<std::byte> image_;
    std::vector<std::filesystem::path> imageFiles_;
    std::size_t imageIndex_{};
    std::atomic<bool> running_{};
    std::atomic<std::uint64_t> frames_{};
    std::atomic<std::uint64_t> dropped_{};
    std::thread thread_;
    [[nodiscard]] bool load_next_image(std::string& error);
    void loop();
};

} // namespace camera::engine

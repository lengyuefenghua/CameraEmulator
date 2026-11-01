#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace camera::infra {

class HttpXmlServer final {
public:
    HttpXmlServer() = default;
    ~HttpXmlServer();
    HttpXmlServer(const HttpXmlServer&) = delete;
    HttpXmlServer& operator=(const HttpXmlServer&) = delete;

    [[nodiscard]] bool start(const std::string& bindIp, std::uint16_t port,
                             std::string cameraId, std::string xml, std::string& error);
    void stop() noexcept;
    [[nodiscard]] bool running() const noexcept { return running_.load(); }
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    void serve();
    std::atomic<bool> running_{};
    std::string bindIp_;
    std::string cameraId_;
    std::string xml_;
    std::uint16_t port_{};
    std::thread thread_;
    void* serverState_{};
};

} // namespace camera::infra

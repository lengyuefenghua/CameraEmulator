#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace camera::infra {

enum class TcpFlag : std::uint8_t { Syn = 0x02, Ack = 0x10, Psh = 0x08, Fin = 0x01 };

struct TcpSegment {
    std::uint32_t sourceAddress{};
    std::uint32_t destinationAddress{};
    std::uint16_t sourcePort{};
    std::uint16_t destinationPort{};
    std::uint32_t sequence{};
    std::uint32_t acknowledgement{};
    TcpFlag flags{};
    std::vector<std::byte> payload;

    [[nodiscard]] bool has(TcpFlag flag) const noexcept {
        return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
    }
};

class TunnelHttpServer final {
public:
    TunnelHttpServer(std::uint32_t cameraAddress, std::uint16_t port, std::string xml);
    [[nodiscard]] std::vector<TcpSegment> receive(const TcpSegment& segment);
    [[nodiscard]] std::vector<TcpSegment> send_due();

private:
    enum class State { SynReceived, Established, ResponseSent, FinSent };
    struct Connection {
        State state{State::SynReceived};
        std::uint32_t nextServerSequence{};
        std::uint32_t nextClientSequence{};
        TcpSegment pending{};
        std::chrono::steady_clock::time_point lastActivity{std::chrono::steady_clock::now()};
        unsigned retransmissions{};
    };
    std::uint32_t cameraAddress_{};
    std::uint16_t port_{};
    std::string xml_;
    std::mutex mutex_;
    std::unordered_map<std::uint64_t, Connection> connections_;
    [[nodiscard]] static std::uint64_t key(std::uint32_t address, std::uint16_t port) noexcept;
};

} // namespace camera::infra

#include "infra/tunnel_http_server.hpp"

#include <format>

namespace camera::infra {
namespace {
constexpr auto kRetransmitInterval = std::chrono::milliseconds(500);
constexpr auto kIdleTimeout = std::chrono::seconds(5);
constexpr unsigned kMaximumRetransmissions = 3;
}

TunnelHttpServer::TunnelHttpServer(std::uint32_t cameraAddress, std::uint16_t port, std::string xml)
    : cameraAddress_(cameraAddress), port_(port), xml_(std::move(xml)) {}

std::uint64_t TunnelHttpServer::key(std::uint32_t address, std::uint16_t port) noexcept {
    return (static_cast<std::uint64_t>(address) << 16) | port;
}

std::vector<TcpSegment> TunnelHttpServer::receive(const TcpSegment& segment) {
    std::lock_guard lock(mutex_);
    if (segment.destinationAddress != cameraAddress_ || segment.destinationPort != port_) return {};
    const auto connectionKey = key(segment.sourceAddress, segment.sourcePort);
    auto connection = connections_.find(connectionKey);
    if (connection == connections_.end()) {
        if (!segment.has(TcpFlag::Syn) || segment.has(TcpFlag::Ack)) return {};
        Connection state;
        state.nextServerSequence = 1;
        state.nextClientSequence = segment.sequence + 1;
        connection = connections_.emplace(connectionKey, state).first;
        auto response = TcpSegment{cameraAddress_, segment.sourceAddress, port_, segment.sourcePort, 1, state.nextClientSequence,
                                   static_cast<TcpFlag>(static_cast<std::uint8_t>(TcpFlag::Syn) | static_cast<std::uint8_t>(TcpFlag::Ack)), {}};
        connection->second.pending = response;
        return {std::move(response)};
    }

    auto& state = connection->second;
    state.lastActivity = std::chrono::steady_clock::now();
    if (state.state == State::SynReceived) {
        if (!segment.has(TcpFlag::Ack) || segment.acknowledgement != state.nextServerSequence + 1) return {};
        state.nextServerSequence++;
        state.pending = {};
        state.retransmissions = 0;
        state.state = State::Established;
        return {};
    }
    if (state.state == State::ResponseSent) {
        if (!segment.has(TcpFlag::Ack) || segment.acknowledgement != state.nextServerSequence) return {};
        state.state = State::FinSent;
        state.pending = {cameraAddress_, segment.sourceAddress, port_, segment.sourcePort, state.nextServerSequence,
                         state.nextClientSequence,
                         static_cast<TcpFlag>(static_cast<std::uint8_t>(TcpFlag::Fin) | static_cast<std::uint8_t>(TcpFlag::Ack)), {}};
        state.retransmissions = 0;
        return {state.pending};
    }
    if (state.state == State::FinSent) {
        if (segment.has(TcpFlag::Ack) && segment.acknowledgement == state.nextServerSequence + 1) connections_.erase(connection);
        return {};
    }
    if (state.state != State::Established || segment.sequence != state.nextClientSequence || segment.payload.empty()) return {};

    state.nextClientSequence += static_cast<std::uint32_t>(segment.payload.size());
    const std::string request(reinterpret_cast<const char*>(segment.payload.data()), segment.payload.size());
    if (!request.starts_with("GET ")) return {};
    const auto response = std::format("HTTP/1.1 200 OK\r\nContent-Type: application/xml\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}", xml_.size(), xml_);
    std::vector<std::byte> payload(response.size());
    for (std::size_t i = 0; i < response.size(); ++i) payload[i] = static_cast<std::byte>(response[i]);
    const auto result = TcpSegment{cameraAddress_, segment.sourceAddress, port_, segment.sourcePort,
                                   state.nextServerSequence, state.nextClientSequence,
                                   static_cast<TcpFlag>(static_cast<std::uint8_t>(TcpFlag::Ack) | static_cast<std::uint8_t>(TcpFlag::Psh)),
                                   std::move(payload)};
    state.nextServerSequence += static_cast<std::uint32_t>(result.payload.size());
    state.pending = result;
    state.retransmissions = 0;
    state.state = State::ResponseSent;
    return {result};
}

std::vector<TcpSegment> TunnelHttpServer::send_due() {
    std::lock_guard lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    std::vector<TcpSegment> due;
    for (auto connection = connections_.begin(); connection != connections_.end();) {
        auto& state = connection->second;
        if (now - state.lastActivity >= kIdleTimeout) {
            connection = connections_.erase(connection);
            continue;
        }
        if (!state.pending.payload.empty() || state.pending.has(TcpFlag::Syn) || state.pending.has(TcpFlag::Fin)) {
            if (now - state.lastActivity >= kRetransmitInterval) {
                if (state.retransmissions == kMaximumRetransmissions) {
                    connection = connections_.erase(connection);
                    continue;
                }
                ++state.retransmissions;
                state.lastActivity = now;
                due.push_back(state.pending);
            }
        }
        ++connection;
    }
    return due;
}

} // namespace camera::infra

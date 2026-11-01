#pragma once

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace camera::infra::win32 {

struct Datagram {
    std::vector<char> bytes;
    std::string sourceIp;
    std::uint16_t sourcePort{};
    std::string destinationIp;
    std::uint32_t interfaceIndex{};
};

class UdpPktinfoTransport final {
public:
    UdpPktinfoTransport() = default;
    ~UdpPktinfoTransport();
    UdpPktinfoTransport(const UdpPktinfoTransport&) = delete;
    UdpPktinfoTransport& operator=(const UdpPktinfoTransport&) = delete;

    [[nodiscard]] bool open(const std::string& bindIp, std::uint16_t port, std::string& error);
    [[nodiscard]] std::optional<Datagram> receive(std::string& error);
    [[nodiscard]] bool send_to(const std::string& destinationIp, std::uint16_t destinationPort,
                               const std::string& sourceIp, const std::vector<char>& bytes,
                               std::string& error);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return socket_ != INVALID_SOCKET; }
    [[nodiscard]] bool send_raw(const std::string& destinationIp, std::uint16_t destinationPort,
                                const std::vector<char>& bytes, std::string& error);
    [[nodiscard]] bool send_to(const std::string& destinationIp, std::uint16_t destinationPort,
                               const std::string& sourceIp, std::span<const std::byte> bytes,
                               std::string& error);

private:
    SOCKET socket_{INVALID_SOCKET};
    bool winsockStarted_{};
    std::uint16_t port_{};
    std::string bindIp_;
};

} // namespace camera::infra::win32

#endif

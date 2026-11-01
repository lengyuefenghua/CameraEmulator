#include "infra/win32/udp_pktinfo_transport.hpp"

#ifdef _WIN32

#include <mstcpip.h>

#include <cstring>
#include <sstream>

namespace camera::infra::win32 {
namespace {

std::string socket_error(const char* operation, const std::string& ip, std::uint16_t port) {
    std::ostringstream out;
    out << "[" << operation << "] ip=" << ip << " port=" << port
        << " WSA=" << WSAGetLastError();
    return out.str();
}

std::string ip_string(DWORD address) {
    IN_ADDR in{};
    in.S_un.S_addr = address;
    char text[INET_ADDRSTRLEN]{};
    if (!InetNtopA(AF_INET, &in, text, static_cast<DWORD>(sizeof(text)))) return "0.0.0.0";
    return text;
}

bool parse_ip(const std::string& text, IN_ADDR& address) {
    return InetPtonA(AF_INET, text.c_str(), &address) == 1;
}

} // namespace

UdpPktinfoTransport::~UdpPktinfoTransport() { close(); }

bool UdpPktinfoTransport::open(const std::string& bindIp, std::uint16_t port, std::string& error) {
    close();
    WSADATA data{};
    const int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0) {
        error = "[bind] ip=" + bindIp + " port=" + std::to_string(port) + " WSA=" + std::to_string(startup);
        return false;
    }
    winsockStarted_ = true;
    socket_ = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (socket_ == INVALID_SOCKET) {
        error = socket_error("socket", bindIp, port);
        close();
        return false;
    }
    BOOL enabled = TRUE;
    if (setsockopt(socket_, IPPROTO_IP, IP_PKTINFO, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
        error = socket_error("IP_PKTINFO", bindIp, port);
        close();
        return false;
    }
    IN_ADDR address{};
    if (!parse_ip(bindIp, address)) {
        error = "[bind] invalid IPv4 interface=" + bindIp + " port=" + std::to_string(port);
        close();
        return false;
    }
    SOCKADDR_IN local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr = address;
    if (bind(socket_, reinterpret_cast<const SOCKADDR*>(&local), sizeof(local)) == SOCKET_ERROR) {
        error = socket_error("bind", bindIp, port);
        close();
        return false;
    }
    bindIp_ = bindIp;
    port_ = port;
    return true;
}

std::optional<Datagram> UdpPktinfoTransport::receive(std::string& error) {
    if (socket_ == INVALID_SOCKET) {
        error = "[receive] socket is closed";
        return std::nullopt;
    }
    DWORD timeout = 50;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        error = socket_error("SO_RCVTIMEO", bindIp_, port_);
        return std::nullopt;
    }
    std::array<char, 65536> payload{};
    std::array<char, 128> control{};
    SOCKADDR_IN source{};
    WSAMSG message{};
    WSABUF buffer{static_cast<ULONG>(payload.size()), payload.data()};
    message.name = reinterpret_cast<SOCKADDR*>(&source);
    message.namelen = sizeof(source);
    message.lpBuffers = &buffer;
    message.dwBufferCount = 1;
    message.Control.buf = control.data();
    message.Control.len = static_cast<ULONG>(control.size());
    LPFN_WSARECVMSG receiveFunction{};
    GUID identifier = WSAID_WSARECVMSG;
    DWORD returned{};
    if (WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &identifier, sizeof(identifier),
                 &receiveFunction, sizeof(receiveFunction), &returned, nullptr, nullptr) == SOCKET_ERROR) {
        error = socket_error("WSAIoctl(WSARecvMsg)", bindIp_, port_);
        return std::nullopt;
    }
    DWORD received{};
    const int result = receiveFunction(socket_, &message, &received, nullptr, nullptr);
    if (result == SOCKET_ERROR) {
        const int code = WSAGetLastError();
        if (code == WSAETIMEDOUT) return std::nullopt;
        std::ostringstream out;
        out << "[receive] ip=" << bindIp_ << " port=" << port_ << " WSA=" << code;
        error = out.str();
        return std::nullopt;
    }
    if ((message.dwFlags & MSG_CTRUNC) != 0) {
        error = "[receive] ancillary metadata truncated bind=" + bindIp_ + " port=" + std::to_string(port_);
        return std::nullopt;
    }

    Datagram datagram;
    datagram.bytes.assign(payload.begin(), payload.begin() + received);
    datagram.sourceIp = ip_string(source.sin_addr.S_un.S_addr);
    datagram.sourcePort = ntohs(source.sin_port);
    for (auto* item = WSA_CMSG_FIRSTHDR(&message); item; item = WSA_CMSG_NXTHDR(&message, item)) {
        if (item->cmsg_level == IPPROTO_IP && item->cmsg_type == IP_PKTINFO && item->cmsg_len >= WSA_CMSG_LEN(sizeof(IN_PKTINFO))) {
            const auto* info = reinterpret_cast<const IN_PKTINFO*>(WSA_CMSG_DATA(item));
            datagram.destinationIp = ip_string(info->ipi_addr.s_addr);
            datagram.interfaceIndex = info->ipi_ifindex;
            break;
        }
    }
    return datagram;
}

bool UdpPktinfoTransport::send_raw(const std::string& destinationIp, std::uint16_t destinationPort,
                                   const std::vector<char>& bytes, std::string& error) {
    if (socket_ == INVALID_SOCKET) {
        error = "[send] socket is closed";
        return false;
    }
    IN_ADDR destination{};
    if (!parse_ip(destinationIp, destination)) {
        error = "[send] invalid IPv4 destination=" + destinationIp + " port=" + std::to_string(destinationPort);
        return false;
    }
    SOCKADDR_IN target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(destinationPort);
    target.sin_addr = destination;
    const int sent = sendto(socket_, bytes.data(), static_cast<int>(bytes.size()), 0,
                            reinterpret_cast<const SOCKADDR*>(&target), sizeof(target));
    if (sent == SOCKET_ERROR || sent != bytes.size()) {
        error = socket_error("send", destinationIp, destinationPort);
        return false;
    }
    return true;
}

bool UdpPktinfoTransport::send_to(const std::string& destinationIp, std::uint16_t destinationPort,
                                  const std::string& sourceIp, const std::vector<char>& bytes,
                                  std::string& error) {
    return send_to(destinationIp, destinationPort, sourceIp,
                   std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), error);
}

bool UdpPktinfoTransport::send_to(const std::string& destinationIp, std::uint16_t destinationPort,
                                  const std::string& sourceIp, std::span<const std::byte> bytes,
                                  std::string& error) {
    if (socket_ == INVALID_SOCKET) {
        error = "[send] socket is closed";
        return false;
    }
    IN_ADDR destination{};
    IN_ADDR source{};
    if (!parse_ip(destinationIp, destination) || !parse_ip(sourceIp, source)) {
        error = "[send] invalid IPv4 source=" + sourceIp + " destination=" + destinationIp +
                " port=" + std::to_string(destinationPort);
        return false;
    }
    SOCKADDR_IN target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(destinationPort);
    target.sin_addr = destination;

    std::array<char, WSA_CMSG_SPACE(sizeof(IN_PKTINFO))> control{};
    WSAMSG message{};
    WSABUF buffer{static_cast<ULONG>(bytes.size()), reinterpret_cast<char*>(const_cast<std::byte*>(bytes.data()))};
    message.name = reinterpret_cast<SOCKADDR*>(&target);
    message.namelen = sizeof(target);
    message.lpBuffers = &buffer;
    message.dwBufferCount = 1;
    message.Control.buf = control.data();
    message.Control.len = static_cast<ULONG>(control.size());
    auto* item = WSA_CMSG_FIRSTHDR(&message);
    item->cmsg_level = IPPROTO_IP;
    item->cmsg_type = IP_PKTINFO;
    item->cmsg_len = WSA_CMSG_LEN(sizeof(IN_PKTINFO));
    auto* info = reinterpret_cast<IN_PKTINFO*>(WSA_CMSG_DATA(item));
    info->ipi_addr = source;
    info->ipi_ifindex = 0;

    LPFN_WSASENDMSG sendFunction{};
    GUID identifier = WSAID_WSASENDMSG;
    DWORD returned{};
    if (WSAIoctl(socket_, SIO_GET_EXTENSION_FUNCTION_POINTER, &identifier, sizeof(identifier),
                 &sendFunction, sizeof(sendFunction), &returned, nullptr, nullptr) == SOCKET_ERROR) {
        error = socket_error("WSAIoctl(WSASendMsg)", sourceIp, port_);
        return false;
    }
    DWORD sent{};
    if (sendFunction(socket_, &message, 0, &sent, nullptr, nullptr) == SOCKET_ERROR || sent != bytes.size()) {
        error = socket_error("send", destinationIp, destinationPort);
        return false;
    }
    return true;
}

void UdpPktinfoTransport::close() noexcept {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    if (winsockStarted_) {
        WSACleanup();
        winsockStarted_ = false;
    }
    bindIp_.clear();
    port_ = 0;
}

} // namespace camera::infra::win32

#endif

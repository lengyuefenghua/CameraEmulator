#include "engine/camera_instance.hpp"

#include "infra/logging.hpp"
#include "protocol/genicam_xml_generator.hpp"
#include "protocol/ipv4_tcp_packet.hpp"
#include "protocol/ipv4_udp_packet.hpp"

using camera::infra::Logging;

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif
#include <string_view>

namespace camera::engine {
namespace {

void log(const char* level, const std::string& source, const std::string& message) {
    const auto parsedLevel = std::string_view(level) == "ERROR" ? spdlog::level::err
        : std::string_view(level) == "WARN" ? spdlog::level::warn : spdlog::level::info;
    Logging::write(parsedLevel, "CameraInstance", source, message);
}

std::string ipv4_to_string(std::uint32_t value) {
    return core::Ipv4Address{static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
                             static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)}.to_string();
}

std::filesystem::path application_directory() {
#ifdef _WIN32
    std::wstring path(MAX_PATH, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

} // namespace

CameraInstance::CameraInstance(core::CameraConfig config)
    : registers_(std::move(config)), session_(registers_),
      dispatcher_(registers_, session_, [this](bool start, std::string& error) {
          if (start) return stream_.start(error);
          stream_.stop();
          return true;
      }),
      xmlServer_(registers_.config().ip.host_value(), registers_.config().httpPort, protocol::GenicamXmlGenerator::generate(registers_)),
      stream_(registers_, peer_) {}

CameraInstance::~CameraInstance() { stop(); }

bool CameraInstance::start(std::string& error) {
    if (running_) return true;
    const auto& config = registers_.config();
    if (!peer_.start(config, application_directory(), error)) {
        log("ERROR", "peer-start", "camera=" + config.id + " " + error);
        return false;
    }
    running_ = true;
    receiveThread_ = std::thread([this] { receive_loop(); });
    log("INFO", "start", "camera=" + config.id + " adapter=" + peer_.info().adapterName +
        " host=" + peer_.info().hostAddress.to_string() + " camera=" + peer_.info().cameraAddress.to_string());
    return true;
}

void CameraInstance::receive_loop() {
    while (running_) {
        std::string error;
        const auto packet = peer_.receive(error);
        if (!packet) {
            if (running_ && !error.empty()) log("ERROR", "receive", "camera=" + registers_.config().id + " " + error);
            for (const auto& response : xmlServer_.send_due()) {
                const protocol::Ipv4TcpSegment responseSegment{response.sourceAddress, response.destinationAddress,
                    response.sourcePort, response.destinationPort, response.sequence, response.acknowledgement,
                    static_cast<std::uint8_t>(response.flags), response.payload};
                const auto responsePacket = protocol::Ipv4TcpPacketCodec::encode(responseSegment);
                if (responsePacket.empty() || !peer_.send(responsePacket, error)) {
                    log("ERROR", "xml-retransmit", "相机=" + registers_.config().id + " XML TCP 重传失败：" + error);
                }
            }
            continue;
        }
        const auto& config = registers_.config();
        if (const auto tcp = protocol::Ipv4TcpPacketCodec::decode(*packet); tcp && tcp->destinationPort == config.httpPort) {
            if (tcp->destinationAddress != config.ip.host_value()) {
                log("WARN", "xml-target", "camera=" + config.id + " destination=" + ipv4_to_string(tcp->destinationAddress));
                continue;
            }
            if (!tcp->payload.empty() && !std::string_view(reinterpret_cast<const char*>(tcp->payload.data()), tcp->payload.size()).starts_with("GET ")) {
                log("WARN", "xml-request", "相机=" + config.id + " 收到不支持的 XML HTTP 请求");
            }
            const infra::TcpSegment incoming{tcp->sourceAddress, tcp->destinationAddress, tcp->sourcePort, tcp->destinationPort,
                                              tcp->sequence, tcp->acknowledgement, static_cast<infra::TcpFlag>(tcp->flags), tcp->payload};
            for (const auto& response : xmlServer_.receive(incoming)) {
                const protocol::Ipv4TcpSegment responseSegment{response.sourceAddress, response.destinationAddress,
                    response.sourcePort, response.destinationPort, response.sequence, response.acknowledgement,
                    static_cast<std::uint8_t>(response.flags), response.payload};
                const auto responsePacket = protocol::Ipv4TcpPacketCodec::encode(responseSegment);
                if (responsePacket.empty() || !peer_.send(responsePacket, error)) {
                    log("ERROR", "xml-send", "camera=" + config.id + " " + error);
                }
            }
            continue;
        }
        const auto udp = protocol::Ipv4UdpPacketCodec::decode(*packet);
        if (!udp || udp->destinationPort != 3956) {
            log("WARN", "packet-drop", "camera=" + config.id + " unsupported IPv4 protocol or destination port");
            continue;
        }
        const auto destination = udp->destinationAddress;
        const auto broadcast = (config.ip.host_value() & config.subnetMask.host_value()) | ~config.subnetMask.host_value();
        if (destination != config.ip.host_value() && destination != 0xffffffffu && destination != broadcast) continue;

        infra::win32::Datagram datagram;
        datagram.bytes.assign(reinterpret_cast<const char*>(udp->payload.data()),
                              reinterpret_cast<const char*>(udp->payload.data() + udp->payload.size()));
        datagram.sourceIp = ipv4_to_string(udp->sourceAddress);
        datagram.sourcePort = udp->sourcePort;
        datagram.destinationIp = ipv4_to_string(destination);
        const auto response = dispatcher_.dispatch(datagram);
        if (!response) continue;
        const std::vector<std::byte> responsePayload(reinterpret_cast<const std::byte*>(response->data()),
                                                     reinterpret_cast<const std::byte*>(response->data() + response->size()));
        const protocol::Ipv4UdpDatagram responseDatagram{config.ip.host_value(), udp->sourceAddress, 3956,
                                                          udp->sourcePort, responsePayload};
        const auto responsePacket = protocol::Ipv4UdpPacketCodec::encode(responseDatagram);
        if (responsePacket.empty() || !peer_.send(responsePacket, error)) {
            log("ERROR", "send", "camera=" + config.id + " destination=" + datagram.sourceIp + " " + error);
        }
        session_.expire_if_idle();
    }
}

void CameraInstance::stop() noexcept {
    if (!running_ && !peer_.is_running()) return;
    running_ = false;
    stream_.stop();
    if (receiveThread_.joinable()) receiveThread_.join();
    peer_.stop();
    log("INFO", "stop", "camera=" + registers_.config().id);
}

} // namespace camera::engine

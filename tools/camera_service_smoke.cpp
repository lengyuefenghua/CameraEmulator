#include "core/camera_model.hpp"
#include "engine/camera_instance.hpp"
#include "engine/engine_facade.hpp"
#include "infra/win32/udp_pktinfo_transport.hpp"
#include "infra/logging.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {
void put16(std::vector<char>& out, unsigned short value) {
    out.push_back(static_cast<char>(value >> 8));
    out.push_back(static_cast<char>(value));
}
void put32(std::vector<char>& out, unsigned int value) {
    out.push_back(static_cast<char>(value >> 24));
    out.push_back(static_cast<char>(value >> 16));
    out.push_back(static_cast<char>(value >> 8));
    out.push_back(static_cast<char>(value));
}
std::vector<char> command(unsigned short opcode, unsigned short id, const std::vector<char>& payload) {
    std::vector<char> result{0x42, 0x01};
    put16(result, opcode);
    put16(result, static_cast<unsigned short>(payload.size()));
    put16(result, id);
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
}

int main(int argc, char** argv) {
    const auto logDirectory = std::filesystem::temp_directory_path() / "camera-emulator-stop-probe";
    camera::infra::Logging::initialize(logDirectory.string());
    camera::core::CameraConfig config;
    const auto configuredIp = argc > 1 ? std::string(argv[1]) : "127.0.0.2";
    config.ip = camera::core::Ipv4Address::parse(configuredIp);
    config.httpPort = argc > 2 ? static_cast<std::uint16_t>(std::stoi(argv[2])) : 18000;
    config.gvspPort = static_cast<std::uint16_t>(config.httpPort + 1);
    config.imagePath = std::filesystem::absolute(std::filesystem::path("assets") / "test-image.bmp").string();
    camera::engine::CameraInstance camera(config);
    std::string error;
    if (!camera.start(error)) {
        std::cerr << error << '\n';
        return 1;
    }

    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return 9;
    camera::infra::win32::UdpPktinfoTransport client;
    if (!client.open(configuredIp, 39580, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    const auto discover = command(0x0002, 1, {});
    if (!client.send_raw(configuredIp, 3956, discover, error)) return 3;
    auto discoveryAck = client.receive(error);
    if (!discoveryAck) {
        std::cerr << "discovery receive failed " << error << '\\n';
        return 4;
    }
    if (discoveryAck->bytes.size() != 0x100 ||
        static_cast<unsigned char>(discoveryAck->bytes[3]) != 3) {
        std::cerr << "discovery failed " << error << '\n';
        return 4;
    }

    std::vector<char> readPayload;
    put32(readPayload, 0x00000000);
    const auto readVersion = command(0x0080, 2, readPayload);
    if (!client.send_raw(configuredIp, 3956, readVersion, error)) return 5;
    auto registerAck = client.receive(error);
    if (!registerAck) {
        std::cerr << "register receive failed " << error << '\\n';
        return 6;
    }
    if (registerAck->bytes.size() != 12 ||
        static_cast<unsigned char>(registerAck->bytes[11]) != 2) {
        std::cerr << "register read failed " << error << '\n';
        return 6;
    }

    const std::string httpRequest = "GET /cam1.xml HTTP/1.1\r\nHost: " + configuredIp + "\r\nConnection: close\r\n\r\n";
    SOCKET http = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(config.httpPort);
    InetPtonA(AF_INET, configuredIp.c_str(), &target.sin_addr);
    if (connect(http, reinterpret_cast<SOCKADDR*>(&target), sizeof(target)) == SOCKET_ERROR) return 7;
    send(http, httpRequest.data(), static_cast<int>(httpRequest.size()), 0);
    char response[8192]{};
    const int count = recv(http, response, sizeof(response), 0);
    closesocket(http);
    camera.stop();
    WSACleanup();
    client.close();
    if (count <= 0 || std::string(response, response + count).find("HTTP/1.1 200 OK") == std::string::npos) return 8;
    camera::infra::Logging::shutdown();
    camera::infra::Logging::write(spdlog::level::info, "Smoke", "after-shutdown", "must be a no-op");

    camera::infra::Logging::initialize(logDirectory.string());
    camera::engine::EngineFacade facade;
    if (!facade.create_camera(config, error)) return 10;
    if (!facade.update_config(config, error)) return 11;
    if (!facade.start(error)) return 12;
    facade.stop();
    camera::infra::Logging::shutdown();
    std::cout << "discovery/register/http/config-replace/stop logging smoke passed\n";
    return 0;
}

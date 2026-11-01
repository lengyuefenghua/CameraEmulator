#include "engine/engine_facade.hpp"
#include "infra/win32/udp_pktinfo_transport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {
void put_be16(std::vector<char>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value >> 8));
    bytes.push_back(static_cast<char>(value));
}

void put_be32(std::vector<char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<char>(value >> 24));
    bytes.push_back(static_cast<char>(value >> 16));
    bytes.push_back(static_cast<char>(value >> 8));
    bytes.push_back(static_cast<char>(value));
}

std::vector<char> write_register(std::uint16_t requestId, std::uint32_t address, std::uint32_t value) {
    std::vector<char> command{0x42, 0x01};
    put_be16(command, 0x0082);
    put_be16(command, 8);
    put_be16(command, requestId);
    put_be32(command, address);
    put_be32(command, value);
    return command;
}

bool send_and_ack(camera::infra::win32::UdpPktinfoTransport& client, const std::string& cameraIp,
                  std::uint16_t requestId, std::uint32_t address, std::uint32_t value, std::string& error) {
    const auto command = write_register(requestId, address, value);
    if (!client.send_raw(cameraIp, 3956, command, error)) return false;
    const auto response = client.receive(error);
    return response && response->bytes.size() >= 8 &&
        static_cast<unsigned char>(response->bytes[0]) == 0 &&
        static_cast<unsigned char>(response->bytes[3]) == 0x83;
}
} // namespace

TEST_CASE("GVCP AcquisitionStart begins a complete GVSP stream", "[GvcpAcquisition]") {
#ifdef _WIN32
    camera::core::CameraConfig config;
    config.ip = {127, 0, 0, 2};
    config.httpPort = 18012;
    config.gvspPort = 18013;
    config.frameRate = 30;
    config.packetSize = 576;
    config.imagePath = std::filesystem::absolute(
        std::filesystem::path(CAMERA_TEST_ASSET_DIR) / "test-image.bmp").string();

    camera::engine::EngineFacade engine;
    std::string error;
    REQUIRE(engine.create_camera(config, error));
    REQUIRE(engine.start(error));

    WSADATA winsock{};
    REQUIRE(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
    camera::infra::win32::UdpPktinfoTransport client;
    const auto streamPort = static_cast<std::uint16_t>(config.gvspPort + 10);
    REQUIRE(client.open("127.0.0.2", streamPort, error));

    REQUIRE(send_and_ack(client, "127.0.0.2", 1, 0x0a00, 2, error));
    REQUIRE(send_and_ack(client, "127.0.0.2", 2, 0x0d18,
                         camera::core::Ipv4Address{127, 0, 0, 2}.host_value(), error));
    REQUIRE(send_and_ack(client, "127.0.0.2", 3, 0x0d00, streamPort, error));
    REQUIRE(send_and_ack(client, "127.0.0.2", 4, 0x0124, 1, error));

    bool sawLeader = false;
    bool sawPayload = false;
    bool sawTrailer = false;
    std::uint16_t gvspSourcePort = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && !sawTrailer) {
        const auto packet = client.receive(error);
        if (!packet || packet->bytes.size() < 8) continue;
        gvspSourcePort = packet->sourcePort;
        const auto content = static_cast<unsigned char>(packet->bytes[4]);
        if (content == 1) sawLeader = true;
        else if (content == 3) sawPayload = true;
        else if (content == 2) sawTrailer = true;
    }

    REQUIRE(send_and_ack(client, "127.0.0.2", 5, 0x0124, 0, error));
    REQUIRE(send_and_ack(client, "127.0.0.2", 6, 0x0d00, 0, error));

    engine.stop();
    client.close();
    WSACleanup();
    REQUIRE(sawLeader);
    REQUIRE(gvspSourcePort == 3957);
    REQUIRE(sawPayload);
    REQUIRE(sawTrailer);
#else
    SUCCEED("Windows-only integration test");
#endif
}

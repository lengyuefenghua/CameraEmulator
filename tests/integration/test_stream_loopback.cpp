#include "engine/engine_facade.hpp"
#include "infra/win32/udp_pktinfo_transport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {
void write_solid_bitmap(const std::filesystem::path& path, unsigned char value) {
    std::array<unsigned char, 70> bitmap{};
    bitmap[0] = 'B';
    bitmap[1] = 'M';
    bitmap[2] = 70;
    bitmap[10] = 54;
    bitmap[14] = 40;
    bitmap[18] = 2;
    bitmap[22] = 2;
    bitmap[26] = 1;
    bitmap[28] = 24;
    bitmap[34] = 16;
    for (std::size_t offset = 54; offset < bitmap.size(); ++offset) bitmap[offset] = value;
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char*>(bitmap.data()), static_cast<std::streamsize>(bitmap.size()));
    REQUIRE(file.good());
}

std::vector<unsigned char> receive_first_payload_pixel(camera::infra::win32::UdpPktinfoTransport& receiver,
                                                       std::string& error) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto packet = receiver.receive(error);
        if (!packet || packet->bytes.size() < 9 || static_cast<unsigned char>(packet->bytes[4]) != 3) continue;
        return {static_cast<unsigned char>(packet->bytes[8])};
    }
    return {};
}
}

TEST_CASE("Started camera streams complete GVSP packets to its configured host", "[StreamLoopback]") {
#ifdef _WIN32
    camera::core::CameraConfig config;
    config.ip = {127, 0, 0, 2};
    config.httpPort = 18010;
    config.gvspPort = 18011;
    config.width = 2;
    config.height = 2;
    config.frameRate = 30;
    config.packetSize = 576;
    config.imagePath = std::filesystem::absolute(std::filesystem::path(CAMERA_TEST_ASSET_DIR) / "test-image.bmp").string();

    camera::engine::EngineFacade engine;
    std::string error;
    REQUIRE(engine.create_camera(config, error));
    REQUIRE(engine.start(error));

    WSADATA winsock{};
    REQUIRE(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
    camera::infra::win32::UdpPktinfoTransport receiver;
    const auto receiverPort = static_cast<std::uint16_t>(config.gvspPort + 10);
    REQUIRE(receiver.open("127.0.0.2", receiverPort, error));
    std::string acquireError;
    REQUIRE(engine.acquire_control("127.0.0.2", 2, acquireError));
    REQUIRE(engine.write_register("127.0.0.2", 0x0d18, camera::core::Ipv4Address{127, 0, 0, 2}.host_value(), acquireError));
    REQUIRE(engine.write_register("127.0.0.2", 0x0d00, receiverPort, acquireError));
    REQUIRE(engine.write_register("127.0.0.2", 0x0124, 1, acquireError));
    REQUIRE(engine.start_acquisition(error));

    bool sawLeader = false;
    bool sawPayload = false;
    bool sawTrailer = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && !sawTrailer) {
        auto packet = receiver.receive(error);
        if (!packet || packet->bytes.size() < 8) continue;
        const auto content = static_cast<unsigned char>(packet->bytes[4]);
        if (content == 1) sawLeader = true;
        else if (content == 3) sawPayload = true;
        else if (content == 2) sawTrailer = true;
    }
    engine.stop_acquisition();
    engine.stop();
    receiver.close();
    WSACleanup();
    REQUIRE(sawLeader);
    REQUIRE(sawPayload);
    REQUIRE(sawTrailer);
#else
    SUCCEED("Windows-only integration test");
#endif
}

TEST_CASE("Directory image source advances sorted BMP frames", "[StreamLoopback]") {
#ifdef _WIN32
    const auto sequenceDirectory = std::filesystem::temp_directory_path() / "camera-emulator-sequence-loopback";
    std::error_code ignored;
    std::filesystem::remove_all(sequenceDirectory, ignored);
    std::filesystem::create_directories(sequenceDirectory);
    write_solid_bitmap(sequenceDirectory / "01-black.bmp", 0);
    write_solid_bitmap(sequenceDirectory / "02-white.bmp", 255);

    camera::core::CameraConfig config;
    config.ip = {127, 0, 0, 2};
    config.httpPort = 18020;
    config.gvspPort = 18021;
    config.width = 2;
    config.height = 2;
    config.frameRate = 5;
    config.packetSize = 576;
    config.imagePath = sequenceDirectory.string();

    camera::engine::EngineFacade engine;
    std::string error;
    REQUIRE(engine.create_camera(config, error));
    REQUIRE(engine.start(error));
    WSADATA winsock{};
    REQUIRE(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
    camera::infra::win32::UdpPktinfoTransport receiver;
    const auto receiverPort = static_cast<std::uint16_t>(config.gvspPort + 10);
    REQUIRE(receiver.open("127.0.0.2", receiverPort, error));
    REQUIRE(engine.acquire_control("127.0.0.2", 2, error));
    REQUIRE(engine.write_register("127.0.0.2", 0x0d18, camera::core::Ipv4Address{127, 0, 0, 2}.host_value(), error));
    REQUIRE(engine.write_register("127.0.0.2", 0x0d00, receiverPort, error));
    const bool started = engine.write_register("127.0.0.2", 0x0124, 1, error);
    const auto first = started ? receive_first_payload_pixel(receiver, error) : std::vector<unsigned char>{};
    std::this_thread::sleep_for(std::chrono::milliseconds(240));
    const auto second = started ? receive_first_payload_pixel(receiver, error) : std::vector<unsigned char>{};

    engine.stop_acquisition();
    engine.stop();
    receiver.close();
    WSACleanup();
    std::filesystem::remove_all(sequenceDirectory, ignored);
    REQUIRE(started);
    REQUIRE(first == std::vector<unsigned char>{0});
    REQUIRE(second == std::vector<unsigned char>{255});
#else
    SUCCEED("Windows-only integration test");
#endif
}

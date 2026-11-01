#include "infra/win32/wintun_peer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
#ifdef _WIN32
bool is_elevated() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size{};
    const auto result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) != FALSE;
    CloseHandle(token);
    return result && elevation.TokenIsElevated != 0;
}

std::filesystem::path executable_directory() {
    std::wstring path(MAX_PATH, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}
#endif
}

TEST_CASE("Wintun peer creates distinct host and camera endpoints then cleans up", "[WintunPeer]") {
#ifdef _WIN32
    if (!is_elevated()) SKIP("Wintun adapter lifecycle requires an elevated Windows test process");

    camera::core::CameraConfig config;
    config.id = "peer-test";
    config.ip = {198, 18, 0, 2};
    config.subnetMask = {255, 255, 255, 0};
    camera::infra::win32::WintunPeer peer;
    std::string error;

    REQUIRE(peer.start(config, executable_directory(), error));
    REQUIRE(peer.is_running());
    REQUIRE(peer.info().hostAddress.host_value() != peer.info().cameraAddress.host_value());
    REQUIRE(peer.info().cameraAddress.host_value() == config.ip.host_value());
    peer.stop();
    REQUIRE_FALSE(peer.is_running());
#else
    SKIP("Wintun is Windows-only");
#endif
}

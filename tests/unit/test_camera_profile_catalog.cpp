#include "core/camera_profile_catalog.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace {
class TemporaryDirectory final {
public:
    explicit TemporaryDirectory(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / name) {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void write_text(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out << content;
}
} // namespace

TEST_CASE("Profile catalog retains invalid JSON profiles with a reason", "[CameraProfileCatalog]") {
    TemporaryDirectory directory("camera-emulator-profile-catalog-invalid");
    write_text(directory.path() / "invalid.json", "{ not valid json");

    const auto profiles = camera::core::CameraProfileCatalog::scan(directory.path());

    REQUIRE(profiles.size() == 1);
    REQUIRE_FALSE(profiles.front().startable);
    REQUIRE_FALSE(profiles.front().error.empty());
}

TEST_CASE("Profile catalog rejects relative image paths", "[CameraProfileCatalog]") {
    TemporaryDirectory directory("camera-emulator-profile-catalog-relative");
    write_text(directory.path() / "relative.json", R"({
        "schemaVersion": 1,
        "id": "relative",
        "vendor": "CameraEmulator",
        "model": "VirtualGigE",
        "serial": "CE000101",
        "version": "1.0.0",
        "manufacturerInfo": "test",
        "ip": "127.0.0.2",
        "subnetMask": "255.0.0.0",
        "gateway": "0.0.0.0",
        "httpPort": 18001,
        "gvspPort": 3957,
        "width": 640,
        "height": 480,
        "pixelFormat": 17301505,
        "packetSize": 1400,
        "packetDelay": 0,
        "heartbeatTimeoutMs": 3000,
        "frameRate": 10,
        "imagePath": "image.bmp"
    })");

    const auto profiles = camera::core::CameraProfileCatalog::scan(directory.path());

    REQUIRE(profiles.size() == 1);
    REQUIRE_FALSE(profiles.front().startable);
}

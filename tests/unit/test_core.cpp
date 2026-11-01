#include "core/camera_model.hpp"
#include "core/validation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("IPv4 address parsing preserves the numeric address", "[Core]") {
    const auto address = camera::core::Ipv4Address::parse("192.168.163.183");
    REQUIRE(address.host_value() == 0xc0a8a3b7u);
    REQUIRE(address.to_string() == "192.168.163.183");
}

TEST_CASE("Camera validation reports loopback deployment warning", "[Core]") {
    camera::core::CameraConfig config;
    config.imagePath = std::filesystem::absolute(std::filesystem::path(CAMERA_TEST_ASSET_DIR) / "test-image.bmp").string();
    const auto result = camera::core::validate(config);
    REQUIRE(result.valid());
    REQUIRE_FALSE(result.warnings.empty());
}

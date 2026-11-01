#pragma once

#include "core/camera_model.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace camera::core {

struct CameraProfile {
    std::filesystem::path filePath;
    CameraConfig config;
    bool startable{};
    std::string error;
};

class CameraProfileCatalog final {
public:
    [[nodiscard]] static std::vector<CameraProfile> scan(const std::filesystem::path& directory);
};

} // namespace camera::core

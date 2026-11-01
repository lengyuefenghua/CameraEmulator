#pragma once

#include "core/camera_model.hpp"

#include <string>
#include <vector>

namespace camera::core {

struct ValidationResult {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] ValidationResult validate(const CameraConfig& config);

} // namespace camera::core

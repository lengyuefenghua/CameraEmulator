#pragma once

#include "core/register_map.hpp"

#include <string>

namespace camera::protocol {

class GenicamXmlGenerator final {
public:
    [[nodiscard]] static std::string generate(const core::RegisterMap& registerMap);
};

} // namespace camera::protocol

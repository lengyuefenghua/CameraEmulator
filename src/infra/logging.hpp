#pragma once

#include <spdlog/spdlog.h>

#include <memory>
#include <string_view>

namespace camera::infra {

class Logging final {
public:
    static void initialize(std::string_view logDirectory);
    static void shutdown() noexcept;
    static void write(spdlog::level::level_enum level, std::string_view module,
                      std::string_view source, std::string_view message) noexcept;

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

} // namespace camera::infra

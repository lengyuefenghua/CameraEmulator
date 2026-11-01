#include "infra/logging.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

namespace camera::infra {
namespace {
std::mutex loggerMutex;
}

std::shared_ptr<spdlog::logger> Logging::logger_;

void Logging::initialize(std::string_view logDirectory) {
    std::lock_guard lock(loggerMutex);
    if (logger_) return;

    if (logDirectory.empty()) throw std::invalid_argument("log directory must not be empty");
    const std::filesystem::path directory{std::string(logDirectory)};
    std::filesystem::create_directories(directory);
    const auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (directory / "camera-emulator.log").string(), 4 * 1024 * 1024, 3, true);
    logger_ = std::make_shared<spdlog::logger>("camera-emulator", fileSink);
    logger_->set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%l%$ %v");
    logger_->set_level(spdlog::level::info);
    logger_->flush_on(spdlog::level::err);
    spdlog::set_default_logger(logger_);
}

void Logging::shutdown() noexcept {
    std::lock_guard lock(loggerMutex);
    if (!logger_) return;
    logger_->flush();
    spdlog::drop("camera-emulator");
    logger_.reset();
}

void Logging::write(spdlog::level::level_enum level, std::string_view module,
                    std::string_view source, std::string_view message) noexcept {
    try {
        std::lock_guard lock(loggerMutex);
        if (logger_) {
            logger_->log(level, "{} {} {}", module, source, message);
            if (level >= spdlog::level::err) logger_->flush();
        }
    } catch (...) {
    }
}

} // namespace camera::infra

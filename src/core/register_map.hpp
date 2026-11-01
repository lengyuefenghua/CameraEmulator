#pragma once

#include "core/camera_model.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace camera::core {

enum class RegisterAccess { ReadOnly, ReadWrite, WriteOnly };
enum class FeatureKind { Integer, Enumeration, Command, String, Boolean };

struct EnumEntry {
    std::string name;
    std::uint32_t value{};
};

struct RegisterSpec {
    std::string name;
    std::uint32_t address{};
    std::uint32_t length{4};
    RegisterAccess access{RegisterAccess::ReadOnly};
};

struct FeatureSpec {
    std::string name;
    std::string displayName;
    std::string category;
    std::string description;
    FeatureKind kind{FeatureKind::Integer};
    std::uint32_t address{};
    std::uint32_t length{4};
    RegisterAccess access{RegisterAccess::ReadOnly};
    std::uint32_t minimum{};
    std::uint32_t maximum{};
    std::uint32_t increment{1};
    std::uint32_t commandValue{};
    std::uint32_t initialValue{};
    std::vector<EnumEntry> enumEntries;
};

class RegisterMap final {
public:
    explicit RegisterMap(CameraConfig config);

    [[nodiscard]] const CameraConfig& config() const noexcept { return config_; }
    [[nodiscard]] const std::vector<FeatureSpec>& features() const noexcept { return features_; }
    [[nodiscard]] const std::vector<RegisterSpec>& registers() const noexcept { return registers_; }
    [[nodiscard]] std::optional<std::uint32_t> read_u32(std::uint32_t address) const;
    [[nodiscard]] std::optional<std::string> read_string(std::uint32_t address, std::uint32_t length) const;
    [[nodiscard]] std::optional<std::string> read_string_range(std::uint32_t address, std::uint32_t length) const;
    [[nodiscard]] bool write_string(std::uint32_t address, std::string value);
    [[nodiscard]] bool write_u32(std::uint32_t address, std::uint32_t value);
    [[nodiscard]] bool write_bytes(std::uint32_t address, std::span<const std::byte> bytes);
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] bool acquisition_running() const noexcept { return acquisitionRunning_; }
    [[nodiscard]] bool loopback_warning() const noexcept { return config_.ip.is_loopback(); }
    void set_stream_port(std::uint16_t port) noexcept;

private:
    struct Cell {
        RegisterSpec spec;
        std::uint32_t value{};
        std::string text;
    };

    CameraConfig config_;
    std::vector<FeatureSpec> features_;
    std::vector<RegisterSpec> registers_;
    std::vector<Cell> cells_;
    std::uint64_t generation_{};
    bool acquisitionRunning_{};

    [[nodiscard]] Cell* find(std::uint32_t address);
    [[nodiscard]] const Cell* find(std::uint32_t address) const;
    void add_scalar(std::string name, std::uint32_t address, std::uint32_t value,
                    RegisterAccess access, std::string feature = {},
                    FeatureKind kind = FeatureKind::Integer,
                    std::uint32_t minimum = 0, std::uint32_t maximum = 0xffffffff,
                    std::vector<EnumEntry> entries = {});
    void add_string(std::string name, std::uint32_t address, std::uint32_t length,
                    std::string value, std::string feature = {});
};

} // namespace camera::core

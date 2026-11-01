#include "core/camera_profile_catalog.hpp"

#include "core/validation.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace camera::core {
namespace {
std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open JSON file");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string string_field(const std::string& json, const char* name) {
    const std::regex field("\\\"" + std::string(name) + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, field)) throw std::runtime_error(std::string("missing or invalid field: ") + name);
    return match[1].str();
}

std::uint32_t unsigned_field(const std::string& json, const char* name) {
    const std::regex field("\\\"" + std::string(name) + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, field)) throw std::runtime_error(std::string("missing or invalid field: ") + name);
    const auto value = std::stoull(match[1].str());
    if (value > 0xffffffffull) throw std::runtime_error(std::string("field exceeds range: ") + name);
    return static_cast<std::uint32_t>(value);
}

std::uint16_t port_field(const std::string& json, const char* name) {
    const auto value = unsigned_field(json, name);
    if (value > 65535u) throw std::runtime_error(std::string("field exceeds port range: ") + name);
    return static_cast<std::uint16_t>(value);
}

bool has_extension(const std::filesystem::path& path, std::string_view expected) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == expected;
}

bool is_bmp(const std::filesystem::path& path) { return has_extension(path, ".bmp"); }

bool is_json(const std::filesystem::path& path) { return has_extension(path, ".json"); }

bool contains_bmp(const std::filesystem::path& directory) {
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(directory, error), end; !error && iterator != end; iterator.increment(error)) {
        std::error_code fileError;
        if (iterator->is_regular_file(fileError) && !fileError && is_bmp(iterator->path())) return true;
    }
    return false;
}

CameraConfig parse_config(const std::string& json) {
    if (unsigned_field(json, "schemaVersion") != 1) throw std::runtime_error("unsupported schemaVersion");
    CameraConfig config;
    config.id = string_field(json, "id");
    config.vendor = string_field(json, "vendor");
    config.model = string_field(json, "model");
    config.serial = string_field(json, "serial");
    config.version = string_field(json, "version");
    config.manufacturerInfo = string_field(json, "manufacturerInfo");
    config.ip = Ipv4Address::parse(string_field(json, "ip"));
    config.subnetMask = Ipv4Address::parse(string_field(json, "subnetMask"));
    config.gateway = Ipv4Address::parse(string_field(json, "gateway"));
    config.httpPort = port_field(json, "httpPort");
    config.gvspPort = port_field(json, "gvspPort");
    config.width = unsigned_field(json, "width");
    config.height = unsigned_field(json, "height");
    config.pixelFormat = static_cast<PixelFormat>(unsigned_field(json, "pixelFormat"));
    config.packetSize = unsigned_field(json, "packetSize");
    config.packetDelay = unsigned_field(json, "packetDelay");
    config.heartbeatTimeoutMs = unsigned_field(json, "heartbeatTimeoutMs");
    config.frameRate = unsigned_field(json, "frameRate");
    config.imagePath = string_field(json, "imagePath");
    return config;
}

void validate_image_source(const CameraConfig& config) {
    const std::filesystem::path imagePath(config.imagePath);
    if (!imagePath.is_absolute()) throw std::runtime_error("imagePath must be absolute");
    std::error_code error;
    if (std::filesystem::is_regular_file(imagePath, error) && !error) {
        if (!is_bmp(imagePath)) throw std::runtime_error("imagePath file must be a BMP");
        return;
    }
    error.clear();
    if (std::filesystem::is_directory(imagePath, error) && !error) {
        if (!contains_bmp(imagePath)) throw std::runtime_error("imagePath directory contains no BMP files");
        return;
    }
    throw std::runtime_error("imagePath does not exist or is not a regular file or directory");
}
} // namespace

std::vector<CameraProfile> CameraProfileCatalog::scan(const std::filesystem::path& directory) {
    std::vector<CameraProfile> profiles;
    std::error_code error;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator iterator(directory, error), end; !error && iterator != end; iterator.increment(error)) {
        std::error_code fileError;
        const auto path = iterator->path();
        if (iterator->is_regular_file(fileError) && !fileError && is_json(path)) files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        CameraProfile profile;
        profile.filePath = file;
        try {
            profile.config = parse_config(read_file(file));
            validate_image_source(profile.config);
            const auto validation = validate(profile.config);
            if (!validation.valid()) throw std::runtime_error(validation.errors.front());
            profile.startable = true;
        } catch (const std::exception& exception) {
            profile.error = exception.what();
        }
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

} // namespace camera::core

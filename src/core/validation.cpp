#include "core/validation.hpp"

#include <filesystem>

namespace camera::core {

ValidationResult validate(const CameraConfig& config) {
    ValidationResult result;
    if (config.id.empty()) result.errors.emplace_back("相机名称不能为空");
    if (config.vendor.empty() || config.model.empty() || config.serial.empty()) {
        result.errors.emplace_back("厂商、型号和序列号不能为空");
    }
    if (config.httpPort == 0 || config.gvspPort == 0 || config.httpPort == config.gvspPort) {
        result.errors.emplace_back("HTTP 与 GVSP 端口必须为不同的有效端口");
    }
    if (config.width == 0 || config.height == 0 || config.width > 16384 || config.height > 16384) {
        result.errors.emplace_back("图像宽高必须在 1 到 16384 之间");
    }
    if (config.packetSize < 576 || config.packetSize > 9000 || (config.packetSize % 4) != 0) {
        result.errors.emplace_back("GVSP 包大小必须为 576 到 9000 且 4 字节对齐");
    }
    if (config.frameRate == 0 || config.frameRate > 240) {
        result.errors.emplace_back("帧率必须在 1 到 240 之间");
    }
    if (config.heartbeatTimeoutMs < 500 || config.heartbeatTimeoutMs > 60000) {
        result.errors.emplace_back("心跳超时必须在 500 到 60000 毫秒之间");
    }
    if (config.pixelFormat != PixelFormat::Mono8 && config.pixelFormat != PixelFormat::RGB8) {
        result.errors.emplace_back("仅支持 Mono8 和 RGB8 像素格式");
    }
    if (config.imagePath.empty()) {
        result.errors.emplace_back("必须配置固定图像源");
    }

    if (!config.imagePath.empty() && !std::filesystem::is_regular_file(config.imagePath) &&
        !std::filesystem::is_directory(config.imagePath)) {
        result.errors.emplace_back("图像源不存在或不是普通文件/目录");
    }
    if (config.ip.is_loopback()) result.warnings.emplace_back("loopback 地址可能被部分 GigE Vision 客户端过滤");
    if (config.ip.a == 0 || config.ip.a >= 224 || config.ip.a == 255) {
        result.errors.emplace_back("相机 IP 必须为可用的单播 IPv4 地址");
    }
    return result;
}

} // namespace camera::core

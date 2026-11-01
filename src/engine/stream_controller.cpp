#include "engine/stream_controller.hpp"

#include "infra/logging.hpp"
#include "protocol/gvsp_codec.hpp"
#include "protocol/ipv4_udp_packet.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <thread>
#include <string_view>

#include <fstream>
#include <limits>

namespace camera::engine {
using camera::infra::Logging;
namespace {
constexpr std::uint16_t kCameraGvspPort = 3957;
constexpr std::uint32_t kStreamIpAddress = 0xd18;
constexpr std::uint32_t kStreamPortAddress = 0xd00;
constexpr std::uint32_t kPacketSizeAddress = 0xd04;
constexpr std::uint32_t kPacketDelayAddress = 0xd08;

std::string ipv4_from_value(std::uint32_t value) {
    core::Ipv4Address ip{static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
                         static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
    return ip.to_string();
}

std::uint16_t read_le16(const unsigned char* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
}

std::uint32_t read_le32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8) |
        (static_cast<std::uint32_t>(bytes[2]) << 16) |
        (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool load_bitmap(const std::string& path, std::uint32_t width, std::uint32_t height,
                 core::PixelFormat format, std::vector<std::byte>& output, std::string& error) {
    if (format != core::PixelFormat::Mono8 && format != core::PixelFormat::RGB8) {
        error = "unsupported output pixel format";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "image file could not be opened"; return false; }
    unsigned char header[54]{};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (file.gcount() != sizeof(header) || header[0] != 'B' || header[1] != 'M' ||
        read_le32(header + 14) < 40 || read_le16(header + 26) != 1) {
        error = "only uncompressed Windows BMP images are supported";
        return false;
    }
    const auto dataOffset = read_le32(header + 10);
    const auto widthBits = read_le32(header + 18);
    const auto heightBits = read_le32(header + 22);
    if (widthBits > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        heightBits == 0x80000000u) {
        error = "BMP dimensions are not representable";
        output.clear();
        return false;
    }
    const auto sourceWidth = static_cast<std::int32_t>(widthBits);
    const auto signedHeight = static_cast<std::int32_t>(heightBits);
    const auto bitsPerPixel = read_le16(header + 28);
    if (sourceWidth <= 0 || signedHeight == 0 || bitsPerPixel != 24 || read_le32(header + 30) != 0) {
        error = "BMP must be 24-bit uncompressed";
        return false;
    }
    const auto sourceHeight = static_cast<std::uint32_t>(signedHeight < 0 ? -static_cast<std::int64_t>(signedHeight) : signedHeight);
    const auto sourceStride = (static_cast<std::uint64_t>(sourceWidth) * 3 + 3) & ~std::uint64_t{3};
    const auto outputChannels = format == core::PixelFormat::RGB8 ? 3u : 1u;
    const auto outputSize = static_cast<std::uint64_t>(width) * height * outputChannels;
    if (sourceStride * sourceHeight > 128ull * 1024 * 1024 || outputSize > 128ull * 1024 * 1024 || dataOffset > 16 * 1024 * 1024) {
        error = "BMP dimensions exceed supported limits";
        return false;
    }
    std::vector<unsigned char> source(static_cast<std::size_t>(sourceStride * sourceHeight));
    file.seekg(dataOffset, std::ios::beg);
    file.read(reinterpret_cast<char*>(source.data()), static_cast<std::streamsize>(source.size()));
    if (!file) { error = "BMP pixel data is truncated"; output.clear(); return false; }
    output.resize(static_cast<std::size_t>(outputSize));
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto sourceY = static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * sourceHeight) / height);
        const auto bitmapY = signedHeight > 0 ? sourceHeight - 1 - sourceY : sourceY;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto sourceX = static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * sourceWidth) / width);
            const auto sourceOffset = static_cast<std::size_t>(bitmapY) * sourceStride + static_cast<std::size_t>(sourceX) * 3;
            const auto target = (static_cast<std::size_t>(y) * width + x) * outputChannels;
            const auto blue = source[sourceOffset];
            const auto green = source[sourceOffset + 1];
            const auto red = source[sourceOffset + 2];
            if (format == core::PixelFormat::RGB8) {
                output[target] = static_cast<std::byte>(red);
                output[target + 1] = static_cast<std::byte>(green);
                output[target + 2] = static_cast<std::byte>(blue);
            } else {
                output[target] = static_cast<std::byte>((77u * red + 150u * green + 29u * blue) >> 8);
            }
        }
    }
    return true;
}

void log_stream(const char* level, const std::string& source, const std::string& message) {
    const auto parsedLevel = std::string_view(level) == "ERROR" ? spdlog::level::err
        : std::string_view(level) == "WARN" ? spdlog::level::warn : spdlog::level::info;
    Logging::write(parsedLevel, "StreamController", source, message);
}

bool is_bmp(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == ".bmp";
}
} // namespace

StreamController::StreamController(core::RegisterMap& registers, infra::win32::WintunPeer& peer)
    : registers_(registers), peer_(peer) {}

StreamController::~StreamController() { stop(); }

bool StreamController::start(std::string& error) {
    if (running_) return true;
    if (thread_.joinable()) {
        if (thread_.get_id() == std::this_thread::get_id()) {
            error = "stream worker cannot restart itself";
            return false;
        }
        thread_.join();
    }
    image_.clear();
    imageFiles_.clear();
    imageIndex_ = 0;
    const auto destination = registers_.read_u32(kStreamIpAddress).value_or(0);
    const auto port = registers_.read_u32(kStreamPortAddress).value_or(0);
    if (destination == 0 || port == 0 || !peer_.is_running()) {
        error = "stream destination or Wintun peer is not configured";
        log_stream("ERROR", "start", "camera=" + registers_.config().id + " " + error);
        return false;
    }
    const auto& config = registers_.config();
    if (!registers_.acquisition_running()) {
        error = "acquisition is not enabled";
        return false;
    }
    const auto expectedSize = static_cast<std::uint64_t>(config.width) * config.height *
        (config.pixelFormat == core::PixelFormat::RGB8 ? 3u : 1u);
    if (expectedSize == 0 || expectedSize > 128ull * 1024 * 1024) {
        error = "configured image dimensions exceed supported frame memory limit";
        return false;
    }
    if (config.imagePath.empty()) {
        error = "a fixed image source is required before acquisition";
        log_stream("ERROR", "image-load", "camera=" + config.id + " " + error);
        return false;
    }
    const std::filesystem::path imagePath(config.imagePath);
    try {
        std::error_code sourceError;
        const bool isDirectory = std::filesystem::is_directory(imagePath, sourceError);
        if (sourceError) {
            error = "image source status could not be read";
                return false;
        }
        if (isDirectory) {
            std::error_code directoryError;
            std::filesystem::directory_iterator iterator(imagePath, directoryError);
            const std::filesystem::directory_iterator end;
            while (!directoryError && iterator != end) {
                const auto candidate = iterator->path();
                std::error_code candidateError;
                if (iterator->is_regular_file(candidateError) && !candidateError && is_bmp(candidate)) imageFiles_.push_back(candidate);
                iterator.increment(directoryError);
            }
            std::sort(imageFiles_.begin(), imageFiles_.end());
            if (directoryError || imageFiles_.empty()) {
                error = directoryError ? "image directory could not be scanned" : "image directory contains no BMP files";
                log_stream("ERROR", "image-load", "camera=" + config.id + " path=" + config.imagePath + " " + error);
                        return false;
            }
        } else if (!load_bitmap(config.imagePath, config.width, config.height, config.pixelFormat, image_, error)) {
            log_stream("ERROR", "image-load", "camera=" + config.id + " path=" + config.imagePath + " " + error);
            image_.clear();
                return false;
        }
    } catch (const std::exception& exception) {
        error = "image source scan failed: " + std::string(exception.what());
        log_stream("ERROR", "image-load", "camera=" + config.id + " path=" + config.imagePath + " " + error);
        image_.clear();
        imageFiles_.clear();
        return false;
    }
    if (!imageFiles_.empty() && imageIndex_ >= imageFiles_.size()) {
        error = "image sequence index is invalid";
        return false;
    }
    frames_ = 0;
    dropped_ = 0;
    running_ = true;
    thread_ = std::thread([this] { loop(); });
    log_stream("INFO", "start", "camera=" + registers_.config().id + " destination=" + ipv4_from_value(destination) + ":" + std::to_string(port));
    return true;
}

void StreamController::stop() noexcept {
    const bool wasRunning = running_.exchange(false);
    if (thread_.joinable()) thread_.join();
    if (!wasRunning) return;
    log_stream("INFO", "stop", "camera=" + registers_.config().id + " frames=" + std::to_string(frames_.load()));
}

bool StreamController::load_next_image(std::string& error) {
    if (imageFiles_.empty()) return true;
    const auto& config = registers_.config();
    const auto initialIndex = imageIndex_;
    do {
        const auto& path = imageFiles_[imageIndex_];
        imageIndex_ = (imageIndex_ + 1) % imageFiles_.size();
        if (load_bitmap(path.string(), config.width, config.height, config.pixelFormat, image_, error)) return true;
        log_stream("WARN", "image-skip", "camera=" + config.id + " path=" + path.string() + " " + error);
    } while (imageIndex_ != initialIndex);
    error = "no readable BMP remains in image directory";
    image_.clear();
    return false;
}

core::RuntimeStatus StreamController::status() const {
    core::RuntimeStatus result;
    result.state = running_ ? core::RuntimeState::Running : core::RuntimeState::Stopped;
    result.frameCount = frames_;
    result.droppedFrames = dropped_;
    return result;
}

void StreamController::loop() {
    try {
    std::uint16_t blockId = 1;
    while (running_) {
        auto frame = pool_.acquire();
        if (!frame) {
            ++dropped_;
            log_stream("WARN", "drop", "camera=" + registers_.config().id + " reason=frame-pool-exhausted");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        const auto& config = registers_.config();
        std::string imageError;
        if (!load_next_image(imageError)) {
            log_stream("ERROR", "image-load", "camera=" + config.id + " path=" + config.imagePath + " " + imageError);
            pool_.release(frame);
            running_ = false;
            break;
        }
        const std::uint32_t bytesPerPixel = config.pixelFormat == core::PixelFormat::RGB8 ? 3u : 1u;
        const auto totalBytes64 = static_cast<std::uint64_t>(config.width) * config.height * bytesPerPixel;
        if (totalBytes64 == 0 || totalBytes64 > 128ull * 1024 * 1024) {
            log_stream("ERROR", "frame", "camera=" + config.id + " invalid-size=" + std::to_string(totalBytes64));
            running_ = false;
            break;
        }
        const auto totalBytes = static_cast<std::size_t>(totalBytes64);
        frame->bytes = image_;
        if (frame->bytes.size() != totalBytes) {
            log_stream("ERROR", "frame", "camera=" + config.id + " preloaded image size does not match register geometry");
            running_ = false;
            break;
        }

        const auto destinationAddress = registers_.read_u32(kStreamIpAddress).value_or(0);
        const auto destination = ipv4_from_value(destinationAddress);
        const auto port = static_cast<std::uint16_t>(registers_.read_u32(kStreamPortAddress).value_or(0));
        const auto configuredPacketSize = registers_.read_u32(kPacketSizeAddress).value_or(1400);
        const auto packetDelay = registers_.read_u32(kPacketDelayAddress).value_or(0);
        const auto payloadCapacity = configuredPacketSize > 36 ? configuredPacketSize - 36 : 0;
        if (payloadCapacity == 0) {
            ++dropped_;
            log_stream("ERROR", "packet-size", "camera=" + config.id + " configured GVSP packet size is smaller than protocol headers");
            pool_.release(frame);
            running_ = false;
            break;
        }
        const protocol::GvspFrameInfo info{0, blockId, config.width, config.height, config.pixelFormat};
        bool sentAll = true;
        std::string error;
        const auto send_gvsp = [&](const std::vector<std::byte>& payload) {
            const protocol::Ipv4UdpDatagram datagram{config.ip.host_value(), destinationAddress, kCameraGvspPort, port, payload};
            const auto ipPacket = protocol::Ipv4UdpPacketCodec::encode(datagram);
            return !ipPacket.empty() && peer_.send(ipPacket, error);
        };
        auto packet = protocol::GvspCodec::leader(info);
        if (!send_gvsp(packet)) sentAll = false;
        std::uint32_t packetId = 1;
        for (std::size_t offset = 0; sentAll && offset < totalBytes; offset += payloadCapacity, ++packetId) {
            if (!running_) { sentAll = false; break; }
            const auto count = std::min<std::size_t>(payloadCapacity, totalBytes - offset);
            packet = protocol::GvspCodec::payload(0, blockId, packetId,
                std::span<const std::byte>(frame->bytes.data() + offset, count), offset + count == totalBytes);
            if (!send_gvsp(packet)) {
                sentAll = false;
                break;
            }
            if (packetDelay) std::this_thread::sleep_for(std::chrono::microseconds(packetDelay));
        }
        if (sentAll) {
            packet = protocol::GvspCodec::trailer(0, blockId, packetId);
            sentAll = send_gvsp(packet);
        }
        if (sentAll) ++frames_;
        else {
            ++dropped_;
            if (!error.empty()) log_stream("ERROR", "send", "camera=" + config.id + " destination=" + destination + ":" + std::to_string(port) + " " + error);
        }
        pool_.release(frame);
        ++blockId;
        if (blockId == 0) blockId = 1;
        const auto frameRate = std::max<std::uint32_t>(1, config.frameRate);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
    }
    } catch (const std::exception& exception) {
        log_stream("ERROR", "worker", "camera=" + registers_.config().id + " exception=" + exception.what());
        running_ = false;
    }
}

} // namespace camera::engine

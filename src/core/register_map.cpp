#include "core/register_map.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace camera::core {
namespace {

constexpr std::uint32_t kVersion = 0x00000000;
constexpr std::uint32_t kDeviceMode = 0x00000004;
constexpr std::uint32_t kMacHigh = 0x00000008;
constexpr std::uint32_t kMacLow = 0x0000000c;
constexpr std::uint32_t kCurrentIp = 0x00000024;
constexpr std::uint32_t kSubnet = 0x00000034;
constexpr std::uint32_t kGateway = 0x00000044;
constexpr std::uint32_t kVendor = 0x00000048;
constexpr std::uint32_t kModel = 0x00000068;
constexpr std::uint32_t kVersionString = 0x00000088;
constexpr std::uint32_t kManufacturerInfo = 0x000000a8;
constexpr std::uint32_t kSerial = 0x000000d8;
constexpr std::uint32_t kUserName = 0x000000e8;
constexpr std::uint32_t kWidth = 0x00000100;
constexpr std::uint32_t kHeight = 0x00000104;
constexpr std::uint32_t kPixelFormat = 0x00000128;
constexpr std::uint32_t kAcquisitionCommand = 0x00000124;
constexpr std::uint32_t kAcquisitionMode = 0x0000012c;
constexpr std::uint32_t kFramePeriod = 0x00000138;
constexpr std::uint32_t kTriggerMode = 0x00000300;
constexpr std::uint32_t kTriggerSource = 0x00000304;
constexpr std::uint32_t kTriggerActivation = 0x00000308;
constexpr std::uint32_t kTriggerSoftware = 0x0000030c;
constexpr std::uint32_t kSensorHeight = 0x00000118;
constexpr std::uint32_t kSensorWidth = 0x0000011c;
constexpr std::uint32_t kChannelCount = 0x00000904;
constexpr std::uint32_t kHeartbeat = 0x00000938;
constexpr std::uint32_t kTimestampFrequencyHigh = 0x0000093c;
constexpr std::uint32_t kTimestampFrequencyLow = 0x00000940;
constexpr std::uint32_t kTimestampControl = 0x00000944;
constexpr std::uint32_t kTimestampHigh = 0x00000948;
constexpr std::uint32_t kTimestampLow = 0x0000094c;
constexpr std::uint32_t kCcp = 0x00000a00;
constexpr std::uint32_t kStreamPort = 0x00000d00;
constexpr std::uint32_t kPacketSize = 0x00000d04;
constexpr std::uint32_t kPacketDelay = 0x00000d08;
constexpr std::uint32_t kStreamIp = 0x00000d18;

std::vector<EnumEntry> pixel_entries() {
    return {{"Mono8", static_cast<std::uint32_t>(PixelFormat::Mono8)},
            {"RGB8", static_cast<std::uint32_t>(PixelFormat::RGB8)}};
}

std::vector<EnumEntry> acquisition_modes() {
    return {{"Continuous", 1}, {"SingleFrame", 2}, {"MultiFrame", 3}};
}

std::vector<EnumEntry> on_off() { return {{"Off", 0}, {"On", 1}}; }

} // namespace

RegisterMap::RegisterMap(CameraConfig config) : config_(std::move(config)) {
    add_scalar("GevVersion", kVersion, 0x00020002, RegisterAccess::ReadOnly);
    add_scalar("GevDeviceMode", kDeviceMode, 0x80000000, RegisterAccess::ReadOnly);
    add_scalar("GevDeviceMACAddressHigh", kMacHigh, 0x00000200, RegisterAccess::ReadOnly);
    add_scalar("GevDeviceMACAddressLow", kMacLow, 0x00000001, RegisterAccess::ReadOnly);
    add_scalar("GevCurrentIPAddress", kCurrentIp, config_.ip.host_value(), RegisterAccess::ReadOnly);
    add_scalar("GevCurrentSubnetMask", kSubnet, config_.subnetMask.host_value(), RegisterAccess::ReadOnly);
    add_scalar("GevCurrentDefaultGateway", kGateway, config_.gateway.host_value(), RegisterAccess::ReadOnly);
    add_string("DeviceVendorName", kVendor, 32, config_.vendor, "DeviceVendorName");
    add_string("DeviceModelName", kModel, 32, config_.model, "DeviceModelName");
    add_string("DeviceVersion", kVersionString, 32, config_.version, "DeviceVersion");
    add_string("DeviceManufacturerInfo", kManufacturerInfo, 48, config_.manufacturerInfo, "DeviceManufacturerInfo");
    add_string("DeviceID", kSerial, 16, config_.serial, "DeviceID");
    add_string("UserDefinedName", kUserName, 16, config_.id);
    add_scalar("SensorHeightRegister", kSensorHeight, config_.height, RegisterAccess::ReadOnly,
               "SensorHeight", FeatureKind::Integer, 1, 16384);
    add_scalar("SensorWidthRegister", kSensorWidth, config_.width, RegisterAccess::ReadOnly,
               "SensorWidth", FeatureKind::Integer, 1, 16384);
    add_scalar("OffsetXRegister", 0x130, 0, RegisterAccess::ReadWrite,
               "OffsetX", FeatureKind::Integer, 0, config_.width);
    add_scalar("OffsetYRegister", 0x134, 0, RegisterAccess::ReadWrite,
               "OffsetY", FeatureKind::Integer, 0, config_.height);
    add_scalar("WidthRegister", kWidth, config_.width, RegisterAccess::ReadWrite,
               "Width", FeatureKind::Integer, 1, 16384);
    add_scalar("HeightRegister", kHeight, config_.height, RegisterAccess::ReadWrite,
               "Height", FeatureKind::Integer, 1, 16384);
    add_scalar("BinningHorizontalRegister", 0x108, 1, RegisterAccess::ReadWrite,
               "BinningHorizontal", FeatureKind::Integer, 1, 16);
    add_scalar("BinningVerticalRegister", 0x10c, 1, RegisterAccess::ReadWrite,
               "BinningVertical", FeatureKind::Integer, 1, 16);
    add_scalar("PixelFormatRegister", kPixelFormat, static_cast<std::uint32_t>(config_.pixelFormat),
               RegisterAccess::ReadWrite, "PixelFormat", FeatureKind::Enumeration, 0, 0xffffffff, pixel_entries());
    add_scalar("AcquisitionCommandRegister", kAcquisitionCommand, 0, RegisterAccess::WriteOnly,
               "AcquisitionStart", FeatureKind::Command, 0, 1);
    features_.push_back({"AcquisitionStop", "Acquisition Stop", "AcquisitionControl", {},
                         FeatureKind::Command, kAcquisitionCommand, 4, RegisterAccess::WriteOnly,
                         0, 0, 1, 0, 0, {}});
    add_scalar("AcquisitionModeRegister", kAcquisitionMode, 1, RegisterAccess::ReadWrite,
               "AcquisitionMode", FeatureKind::Enumeration, 1, 3, acquisition_modes());
    add_scalar("TriggerModeRegister", kTriggerMode, 0, RegisterAccess::ReadWrite,
               "TriggerMode", FeatureKind::Enumeration, 0, 1, on_off());
    add_scalar("TriggerSourceRegister", kTriggerSource, 1, RegisterAccess::ReadWrite,
               "TriggerSource", FeatureKind::Enumeration, 0, 1, {{"Line0", 0}, {"Software", 1}});
    add_scalar("TriggerActivationRegister", kTriggerActivation, 0, RegisterAccess::ReadWrite,
               "TriggerActivation", FeatureKind::Enumeration, 0, 2,
               {{"RisingEdge", 0}, {"FallingEdge", 1}, {"AnyEdge", 2}});
    add_scalar("TriggerSoftwareRegister", kTriggerSoftware, 0, RegisterAccess::WriteOnly,
               "TriggerSoftware", FeatureKind::Command, 0, 1);
    add_scalar("GevStreamChannelCountRegister", kChannelCount, 1, RegisterAccess::ReadOnly,
               "GevStreamChannelCount", FeatureKind::Integer, 1, 1);
    add_scalar("GevHeartbeatTimeoutRegister", kHeartbeat, config_.heartbeatTimeoutMs,
               RegisterAccess::ReadWrite, "GevHeartbeatTimeout", FeatureKind::Integer, 500, 60000);
    add_scalar("GevTimestampTickFrequencyHighRegister", kTimestampFrequencyHigh, 0,
               RegisterAccess::ReadOnly);
    add_scalar("GevTimestampTickFrequencyLowRegister", kTimestampFrequencyLow, 1000000,
               RegisterAccess::ReadOnly);
    add_scalar("GevTimestampControlRegister", kTimestampControl, 0, RegisterAccess::WriteOnly,
               "TimestampControlLatch", FeatureKind::Command, 0, 2);
    add_scalar("GevTimestampValueHighRegister", kTimestampHigh, 0, RegisterAccess::ReadOnly);
    add_scalar("GevTimestampValueLowRegister", kTimestampLow, 0, RegisterAccess::ReadOnly);
    add_scalar("GevCCPRegister", kCcp, 0, RegisterAccess::ReadWrite,
               "GevCCP", FeatureKind::Enumeration, 0, 3,
               {{"OpenAccess", 0}, {"ExclusiveAccess", 1}, {"ControlAccess", 2}, {"ExclusiveControlAccess", 3}});
    features_.push_back({"GevTimestampTickFrequency", "Timestamp Tick Frequency", "TransportLayerControl", {},
                         FeatureKind::Integer, kTimestampFrequencyLow, 4, RegisterAccess::ReadOnly,
                         1, 0xffffffff, 1, 0, 1000000, {}});
    features_.push_back({"GevTimestampValue", "Timestamp Value", "TransportLayerControl", {},
                         FeatureKind::Integer, kTimestampLow, 4, RegisterAccess::ReadOnly,
                         0, 0xffffffff, 1, 0, 0, {}});
    features_.push_back({"GevTimestampControlLatch", "Timestamp Control Latch", "TransportLayerControl", {},
                         FeatureKind::Command, kTimestampControl, 4, RegisterAccess::WriteOnly,
                         0, 2, 1, 2, 0, {}});
    add_scalar("GevStreamChannel0PortRegister", kStreamPort, config_.gvspPort, RegisterAccess::ReadWrite,
               "GevSCPHostPort", FeatureKind::Integer, 0, 65535);
    add_scalar("GevStreamChannel0PacketSizeRegister", kPacketSize, config_.packetSize,
               RegisterAccess::ReadWrite, "GevSCPSPacketSize", FeatureKind::Integer, 576, 9000);
    add_scalar("GevStreamChannel0PacketDelayRegister", kPacketDelay, config_.packetDelay,
               RegisterAccess::ReadWrite, "GevSCPD", FeatureKind::Integer, 0, 0xffffffff);
    add_scalar("GevStreamChannel0IPAddressRegister", kStreamIp, 0,
               RegisterAccess::ReadWrite, "GevSCDA", FeatureKind::Integer, 0, 0xffffffff);
    features_.push_back({"TriggerSelector", "Trigger Selector", "AcquisitionControl", {},
                         FeatureKind::Enumeration, kTriggerMode, 4, RegisterAccess::ReadWrite,
                         0, 1, 1, 0, 0, {{"AcquisitionStart", 0}, {"FrameStart", 1}}});
    add_scalar("AcquisitionFramePeriodRegister", kFramePeriod, 1000000 / config_.frameRate,
               RegisterAccess::ReadWrite, "AcquisitionFrameRate", FeatureKind::Integer,
               1, 1000000);
}

RegisterMap::Cell* RegisterMap::find(std::uint32_t address) {
    const auto it = std::find_if(cells_.begin(), cells_.end(), [address](const auto& cell) {
        return cell.spec.address == address;
    });
    return it == cells_.end() ? nullptr : &*it;
}

const RegisterMap::Cell* RegisterMap::find(std::uint32_t address) const {
    const auto it = std::find_if(cells_.begin(), cells_.end(), [address](const auto& cell) {
        return cell.spec.address == address;
    });
    return it == cells_.end() ? nullptr : &*it;
}

void RegisterMap::add_scalar(std::string name, std::uint32_t address, std::uint32_t value,
                             RegisterAccess access, std::string feature, FeatureKind kind,
                             std::uint32_t minimum, std::uint32_t maximum,
                             std::vector<EnumEntry> entries) {
    RegisterSpec spec{name, address, 4, access};
    registers_.push_back(spec);
    cells_.push_back({spec, value, {}});
    if (feature.empty()) return;
    FeatureSpec featureSpec{feature, feature, {}, {}, kind, address, 4, access,
                            minimum, maximum, 1, 1, value, std::move(entries)};
    if (feature == "Width" || feature == "Height" || feature == "PixelFormat") {
        featureSpec.category = "ImageFormatControl";
    } else if (feature.starts_with("Acquisition") || feature.starts_with("Trigger")) {
        featureSpec.category = "AcquisitionControl";
    } else if (feature.starts_with("Gev")) {
        featureSpec.category = "TransportLayerControl";
    } else {
        featureSpec.category = "DeviceControl";
    }
    features_.push_back(std::move(featureSpec));
}

void RegisterMap::add_string(std::string name, std::uint32_t address, std::uint32_t length,
                             std::string value, std::string feature) {
    RegisterSpec spec{name, address, length, RegisterAccess::ReadOnly};
    registers_.push_back(spec);
    cells_.push_back({spec, 0, std::move(value)});
    if (!feature.empty()) {
        features_.push_back({feature, feature, "DeviceControl", feature,
                             FeatureKind::String, address, length, RegisterAccess::ReadOnly,
                             0, 0, 1, 0, 0, {}});
    }
}

std::optional<std::uint32_t> RegisterMap::read_u32(std::uint32_t address) const {
    const auto* cell = find(address);
    if (!cell || cell->spec.length != 4 || cell->spec.access == RegisterAccess::WriteOnly) return std::nullopt;
    return cell->value;
}

std::optional<std::string> RegisterMap::read_string(std::uint32_t address, std::uint32_t length) const {
    const auto* cell = find(address);
    if (!cell || cell->spec.length != length) return std::nullopt;
    return cell->text;
}

std::optional<std::string> RegisterMap::read_string_range(std::uint32_t address, std::uint32_t length) const {
    for (const auto& cell : cells_) {
        const auto begin = cell.spec.address;
        const auto end = begin + cell.spec.length;
        if (address >= begin && address <= end && length <= end - address && !cell.text.empty()) {
            if (address == begin && length == cell.spec.length) return cell.text;
            const auto offset = static_cast<std::size_t>(address - begin);
            return cell.text.substr(offset, length);
        }
    }
    return std::nullopt;
}

bool RegisterMap::write_string(std::uint32_t address, std::string value) {
    auto* cell = find(address);
    if (!cell || cell->spec.access == RegisterAccess::ReadOnly || value.size() > cell->spec.length) return false;
    cell->text = std::move(value);
    ++generation_;
    return true;
}

bool RegisterMap::write_u32(std::uint32_t address, std::uint32_t value) {
    auto* cell = find(address);
    if (!cell || cell->spec.length != 4 || cell->spec.access == RegisterAccess::ReadOnly) return false;
    if (address == kAcquisitionCommand) {
        if (value > 1) return false;
        acquisitionRunning_ = value == 1;
    } else if (address == kTriggerSoftware) {
        return false;
    } else if (address == kTimestampControl) {
        if (value > 2) return false;
        auto* high = find(kTimestampHigh);
        auto* low = find(kTimestampLow);
        const auto ticks = static_cast<std::uint64_t>(generation_) * 100000;
        high->value = static_cast<std::uint32_t>(ticks >> 32);
        low->value = static_cast<std::uint32_t>(ticks);
    } else {
        auto feature = std::find_if(features_.begin(), features_.end(), [address](const auto& item) {
            return item.address == address;
        });
        if (feature != features_.end() && (value < feature->minimum || value > feature->maximum)) return false;
        cell->value = value;
        if (address == kWidth) config_.width = value;
        if (address == kHeight) config_.height = value;
        if (address == kHeartbeat) config_.heartbeatTimeoutMs = value;
        if (address == kPacketSize) config_.packetSize = value;
        if (address == kPacketDelay) config_.packetDelay = value;
        if (address == kStreamPort) config_.gvspPort = static_cast<std::uint16_t>(value);
    }
    ++generation_;
    return true;
}

bool RegisterMap::write_bytes(std::uint32_t address, std::span<const std::byte> bytes) {
    auto* cell = find(address);
    if (!cell || cell->spec.access == RegisterAccess::ReadOnly || bytes.size() > cell->spec.length) return false;
    if (cell->spec.length == 4 && bytes.size() == 4) {
        const auto value = (std::to_integer<std::uint32_t>(bytes[0]) << 24) |
                           (std::to_integer<std::uint32_t>(bytes[1]) << 16) |
                           (std::to_integer<std::uint32_t>(bytes[2]) << 8) |
                           std::to_integer<std::uint32_t>(bytes[3]);
        return write_u32(address, value);
    }
    return false;
}

void RegisterMap::set_stream_port(std::uint16_t port) noexcept {
    if (auto* cell = find(kStreamPort)) cell->value = port;
}

} // namespace camera::core

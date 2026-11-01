#include "infra/win32/wintun_peer.hpp"

#ifdef _WIN32

#include "infra/logging.hpp"
#include "wintun.h"

#include <iphlpapi.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <thread>

namespace camera::infra::win32 {
namespace {

constexpr DWORD kRingCapacity = 0x400000;

std::string win32_error(DWORD code) {
    return std::format("win32={}", code);
}

void log(spdlog::level::level_enum level, std::string_view source, std::string_view message) noexcept {
    camera::infra::Logging::write(level, "WintunPeer", source, message);
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const auto length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
    return result;
}

std::string sanitize_name(std::string value) {
    for (auto& character : value) {
        if (!std::isalnum(static_cast<unsigned char>(character))) character = '-';
    }
    if (value.empty()) value = "camera";
    value.resize(std::min<std::size_t>(value.size(), 36));
    return value;
}

bool subnet_route_exists(const core::CameraConfig& config, std::uint32_t prefix) {
    PMIB_IPFORWARD_TABLE2 table{};
    if (GetIpForwardTable2(AF_INET, &table) != NO_ERROR) return false;
    const auto network = config.ip.host_value() & config.subnetMask.host_value();
    const auto found = std::any_of(table->Table, table->Table + table->NumEntries, [network, prefix](const auto& row) {
        return row.DestinationPrefix.PrefixLength == prefix &&
               ntohl(row.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr) == network;
    });
    FreeMibTable(table);
    return found;
}

std::uint32_t mask_prefix(const core::Ipv4Address& mask) {
    std::uint32_t value = mask.host_value();
    std::uint32_t prefix{};
    while ((value & 0x80000000u) != 0) {
        ++prefix;
        value <<= 1;
    }
    return value == 0 ? prefix : 0;
}

bool address_exists(std::uint32_t address) {
    PMIB_UNICASTIPADDRESS_TABLE table{};
    if (GetUnicastIpAddressTable(AF_INET, &table) != NO_ERROR) return false;
    const auto found = std::any_of(table->Table, table->Table + table->NumEntries, [address](const auto& entry) {
        return entry.Address.Ipv4.sin_addr.S_un.S_addr == htonl(address);
    });
    FreeMibTable(table);
    return found;
}

core::Ipv4Address derive_host_address(const core::CameraConfig& config) {
    const auto mask = config.subnetMask.host_value();
    const auto network = config.ip.host_value() & mask;
    const auto broadcast = network | ~mask;
    for (std::uint32_t host = 1; host < 65536 && network + host < broadcast; ++host) {
        const auto candidate = network + host;
        if (candidate != config.ip.host_value() && !address_exists(candidate)) {
            return {static_cast<std::uint8_t>(candidate >> 24), static_cast<std::uint8_t>(candidate >> 16),
                    static_cast<std::uint8_t>(candidate >> 8), static_cast<std::uint8_t>(candidate)};
        }
    }
    return {};
}

} // namespace

struct WintunPeer::Implementation {
    HMODULE module{};
    WINTUN_ADAPTER_HANDLE adapter{};
    WINTUN_SESSION_HANDLE session{};
    NET_LUID luid{};
    MIB_UNICASTIPADDRESS_ROW address{};
    MIB_IPFORWARD_ROW2 route{};
    bool addressCreated{};
    bool routeCreated{};
    bool active{};
    WINTUN_CREATE_ADAPTER_FUNC* createAdapter{};
    WINTUN_CLOSE_ADAPTER_FUNC* closeAdapter{};
    WINTUN_GET_ADAPTER_LUID_FUNC* getAdapterLuid{};
    WINTUN_START_SESSION_FUNC* startSession{};
    WINTUN_END_SESSION_FUNC* endSession{};
    WINTUN_GET_READ_WAIT_EVENT_FUNC* getReadWaitEvent{};
    WINTUN_RECEIVE_PACKET_FUNC* receivePacket{};
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* releaseReceivePacket{};
    WINTUN_ALLOCATE_SEND_PACKET_FUNC* allocateSendPacket{};
    WINTUN_SEND_PACKET_FUNC* sendPacket{};
    std::mutex mutex;
};

WintunPeer::~WintunPeer() { stop(); }

bool WintunPeer::start(const core::CameraConfig& config, const std::filesystem::path& applicationDirectory,
                       std::string& error) {
    stop();
    const auto prefix = mask_prefix(config.subnetMask);
    if (prefix == 0 || prefix > 30 || config.ip.host_value() == 0) {
        error = "无效的相机 IPv4 子网";
        return false;
    }
    if (subnet_route_exists(config, prefix)) {
        error = "相机 IPv4 子网已由 Windows 现有路由使用";
        return false;
    }
    if (address_exists(config.ip.host_value())) {
        error = "相机端 IPv4 已被 Windows 本机接口使用";
        return false;
    }
    const auto host = derive_host_address(config);
    if (host.host_value() == 0 || host.host_value() == config.ip.host_value()) {
        error = "无法从相机子网导出不同且未被使用的主机端 IPv4";
        return false;
    }

    auto implementation = std::make_shared<Implementation>();
    const auto modulePath = applicationDirectory / "wintun.dll";
    implementation->module = LoadLibraryExW(modulePath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    if (!implementation->module) {
        error = "无法加载 Wintun.dll " + win32_error(GetLastError());
        log(spdlog::level::err, "load", error);
        return false;
    }
    const auto resolve = [&implementation](const char* name) -> FARPROC {
        return GetProcAddress(implementation->module, name);
    };
    implementation->createAdapter = reinterpret_cast<WINTUN_CREATE_ADAPTER_FUNC*>(resolve("WintunCreateAdapter"));
    implementation->closeAdapter = reinterpret_cast<WINTUN_CLOSE_ADAPTER_FUNC*>(resolve("WintunCloseAdapter"));
    implementation->getAdapterLuid = reinterpret_cast<WINTUN_GET_ADAPTER_LUID_FUNC*>(resolve("WintunGetAdapterLUID"));
    implementation->startSession = reinterpret_cast<WINTUN_START_SESSION_FUNC*>(resolve("WintunStartSession"));
    implementation->endSession = reinterpret_cast<WINTUN_END_SESSION_FUNC*>(resolve("WintunEndSession"));
    implementation->getReadWaitEvent = reinterpret_cast<WINTUN_GET_READ_WAIT_EVENT_FUNC*>(resolve("WintunGetReadWaitEvent"));
    implementation->receivePacket = reinterpret_cast<WINTUN_RECEIVE_PACKET_FUNC*>(resolve("WintunReceivePacket"));
    implementation->releaseReceivePacket = reinterpret_cast<WINTUN_RELEASE_RECEIVE_PACKET_FUNC*>(resolve("WintunReleaseReceivePacket"));
    implementation->allocateSendPacket = reinterpret_cast<WINTUN_ALLOCATE_SEND_PACKET_FUNC*>(resolve("WintunAllocateSendPacket"));
    implementation->sendPacket = reinterpret_cast<WINTUN_SEND_PACKET_FUNC*>(resolve("WintunSendPacket"));
    if (!implementation->createAdapter || !implementation->closeAdapter || !implementation->getAdapterLuid ||
        !implementation->startSession || !implementation->endSession || !implementation->getReadWaitEvent ||
        !implementation->receivePacket || !implementation->releaseReceivePacket || !implementation->allocateSendPacket ||
        !implementation->sendPacket) {
        error = "Wintun.dll 缺少所需 API";
        log(spdlog::level::err, "resolve", error);
        FreeLibrary(implementation->module);
        return false;
    }

    const auto adapterName = "CameraEmulator-" + sanitize_name(config.id) + "-" +
                             std::to_string(GetCurrentProcessId());
    const auto wideName = utf8_to_wide(adapterName);
    implementation->adapter = implementation->createAdapter(wideName.c_str(), L"CameraEmulator", nullptr);
    if (!implementation->adapter) {
        error = "创建 Wintun 适配器失败 " + win32_error(GetLastError());
        log(spdlog::level::err, "create", error);
        FreeLibrary(implementation->module);
        return false;
    }
    implementation->getAdapterLuid(implementation->adapter, &implementation->luid);

    InitializeUnicastIpAddressEntry(&implementation->address);
    implementation->address.InterfaceLuid = implementation->luid;
    implementation->address.Address.si_family = AF_INET;
    implementation->address.Address.Ipv4.sin_addr.S_un.S_addr = htonl(host.host_value());
    implementation->address.OnLinkPrefixLength = static_cast<UINT8>(prefix);
    if (const auto status = CreateUnicastIpAddressEntry(&implementation->address); status != NO_ERROR) {
        error = "配置 Wintun 主机端 IPv4 失败 " + win32_error(status);
        log(spdlog::level::err, "address", error);
        implementation->closeAdapter(implementation->adapter);
        FreeLibrary(implementation->module);
        return false;
    }
    implementation->addressCreated = true;

    InitializeIpForwardEntry(&implementation->route);
    implementation->route.InterfaceLuid = implementation->luid;
    implementation->route.DestinationPrefix.Prefix.si_family = AF_INET;
    implementation->route.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr = htonl(config.ip.host_value());
    implementation->route.DestinationPrefix.PrefixLength = 32;
    implementation->route.NextHop.si_family = AF_INET;
    implementation->route.Metric = 1;
    if (const auto status = CreateIpForwardEntry2(&implementation->route); status != NO_ERROR) {
        error = "配置相机端 IPv4 路由失败 " + win32_error(status);
        log(spdlog::level::err, "route", error);
        DeleteUnicastIpAddressEntry(&implementation->address);
        implementation->closeAdapter(implementation->adapter);
        FreeLibrary(implementation->module);
        return false;
    }
    implementation->routeCreated = true;

    implementation->session = implementation->startSession(implementation->adapter, kRingCapacity);
    if (!implementation->session) {
        error = "启动 Wintun 会话失败 " + win32_error(GetLastError());
        log(spdlog::level::err, "session", error);
        DeleteIpForwardEntry2(&implementation->route);
        DeleteUnicastIpAddressEntry(&implementation->address);
        implementation->closeAdapter(implementation->adapter);
        FreeLibrary(implementation->module);
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (GetUnicastIpAddressEntry(&implementation->address) == NO_ERROR &&
           implementation->address.DadState == IpDadStateTentative && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (implementation->address.DadState != IpDadStatePreferred) {
        error = "Wintun 主机端 IPv4 未进入可用状态";
        log(spdlog::level::err, "address-ready", error);
        implementation->endSession(implementation->session);
        DeleteIpForwardEntry2(&implementation->route);
        DeleteUnicastIpAddressEntry(&implementation->address);
        implementation->closeAdapter(implementation->adapter);
        FreeLibrary(implementation->module);
        return false;
    }
    info_ = {adapterName, host, config.ip};
    implementation->active = true;
    {
        std::lock_guard lock(implementationMutex_);
        implementation_ = implementation;
    }
    log(spdlog::level::info, "start", std::format("camera={} adapter={} host={} camera={}", config.id,
                                                    info_.adapterName, info_.hostAddress.to_string(),
                                                    info_.cameraAddress.to_string()));
    return true;
}

std::optional<std::vector<std::byte>> WintunPeer::receive(std::string& error) {
    std::shared_ptr<Implementation> implementation;
    {
        std::lock_guard lock(implementationMutex_);
        implementation = implementation_;
    }
    if (!implementation) return std::nullopt;
    std::lock_guard lock(implementation->mutex);
    if (!implementation->active) return std::nullopt;
    DWORD size{};
    auto* packet = implementation->receivePacket(implementation->session, &size);
    if (!packet) {
        const auto status = GetLastError();
        if (status == ERROR_NO_MORE_ITEMS) {
            // Wintun 会话结束前持锁最多 100ms，避免关闭其管理的等待事件时发生竞态。
            (void)WaitForSingleObject(implementation->getReadWaitEvent(implementation->session), 100);
        } else {
            error = "Wintun 接收失败 " + win32_error(status);
        }
        return std::nullopt;
    }
    std::vector<std::byte> result(size);
    std::memcpy(result.data(), packet, size);
    implementation->releaseReceivePacket(implementation->session, packet);
    return result;
}

bool WintunPeer::send(std::span<const std::byte> packet, std::string& error) {
    if (packet.empty() || packet.size() > WINTUN_MAX_IP_PACKET_SIZE) {
        error = "Wintun 发送报文无效";
        return false;
    }
    std::shared_ptr<Implementation> implementation;
    {
        std::lock_guard lock(implementationMutex_);
        implementation = implementation_;
    }
    if (!implementation) {
        error = "Wintun peer 未运行";
        return false;
    }
    std::lock_guard lock(implementation->mutex);
    if (!implementation->active) {
        error = "Wintun peer 正在停止";
        return false;
    }
    auto* target = implementation->allocateSendPacket(implementation->session, static_cast<DWORD>(packet.size()));
    if (!target) {
        error = "Wintun 分配发送缓冲失败 " + win32_error(GetLastError());
        return false;
    }
    std::memcpy(target, packet.data(), packet.size());
    implementation->sendPacket(implementation->session, target);
    return true;
}

void WintunPeer::stop() noexcept {
    std::shared_ptr<Implementation> implementation;
    {
        std::lock_guard lock(implementationMutex_);
        implementation = std::move(implementation_);
    }
    if (!implementation) return;
    std::lock_guard lock(implementation->mutex);
    implementation->active = false;
    if (implementation->session) implementation->endSession(implementation->session);
    if (implementation->routeCreated) {
        if (const auto status = DeleteIpForwardEntry2(&implementation->route); status != NO_ERROR) {
            log(spdlog::level::err, "route-delete", win32_error(status));
        }
    }
    if (implementation->addressCreated) {
        if (const auto status = DeleteUnicastIpAddressEntry(&implementation->address); status != NO_ERROR) {
            log(spdlog::level::err, "address-delete", win32_error(status));
        }
    }
    if (implementation->adapter) implementation->closeAdapter(implementation->adapter);
    if (implementation->module) FreeLibrary(implementation->module);
    log(spdlog::level::info, "stop", std::format("adapter={}", info_.adapterName));
    info_ = {};
}

bool WintunPeer::is_running() const noexcept {
    std::lock_guard lock(implementationMutex_);
    return implementation_ != nullptr;
}

} // namespace camera::infra::win32

#endif

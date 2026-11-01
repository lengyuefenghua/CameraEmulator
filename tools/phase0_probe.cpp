#include "infra/win32/udp_pktinfo_transport.hpp"

#ifdef _WIN32

#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void log(const char* level, const std::string& message) {
    const auto now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    std::cout << std::put_time(&local, "%Y-%m-%dT%H:%M:%S%z")
              << ' ' << level << " Phase0Probe local " << message << '\n';
}

bool probe(const std::string& address, std::uint16_t port) {
    camera::infra::win32::UdpPktinfoTransport receiver;
    std::string error;
    if (!receiver.open(address, port, error)) {
        log("ERROR", error);
        return false;
    }
    camera::infra::win32::UdpPktinfoTransport sender;
    if (!sender.open(address, static_cast<std::uint16_t>(port + 1), error)) {
        log("ERROR", error);
        return false;
    }

    std::array<char, 8> payload{'p', 'k', 't', 'i', 'n', 'f', 'o', '!' };
    bool sent = false;
    std::thread sendThread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sent = sender.send_to(address, port, address, std::vector<char>(payload.begin(), payload.end()), error);
    });
    auto received = receiver.receive(error);
    sendThread.join();
    if (!sent || !received) {
        log("ERROR", error.empty() ? "probe send/receive failed" : error);
        return false;
    }
    log("INFO", "bind=" + address + ":" + std::to_string(port) +
                 " src=" + received->sourceIp + ":" + std::to_string(received->sourcePort) +
                 " dst=" + received->destinationIp + " ifindex=" + std::to_string(received->interfaceIndex) +
                 " bytes=" + std::to_string(received->bytes.size()));
    return received->bytes.size() == payload.size() && received->destinationIp == address;
}

} // namespace

int main(int argc, char* argv[]) {
    bool ok = true;
    ok = probe("127.0.0.2", 39570) && ok;
    if (argc > 1) ok = probe(argv[1], 39572) && ok;
    else std::cout << "WLAN IPv4 探测地址请作为首个参数传入。\n";
    return ok ? 0 : 1;
}

#else
int main() { return 2; }
#endif

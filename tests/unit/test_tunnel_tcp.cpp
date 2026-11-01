#include "infra/tunnel_http_server.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace {
camera::infra::TcpSegment syn(std::uint32_t host, std::uint32_t camera, std::uint16_t port) {
    return {host, camera, port, 8000, 100, 0, camera::infra::TcpFlag::Syn, {}};
}
}

TEST_CASE("Tunnel HTTP server completes handshake and returns XML to GET", "[TunnelTcp]") {
    camera::infra::TunnelHttpServer server(0xa9fe6d11u, 8000, "<RegisterDescription/>");
    const auto synAck = server.receive(syn(0xa9fe6d10u, 0xa9fe6d11u, 51000));

    REQUIRE(synAck.size() == 1);
    REQUIRE(synAck.front().has(camera::infra::TcpFlag::Syn));
    REQUIRE(synAck.front().has(camera::infra::TcpFlag::Ack));
    REQUIRE(synAck.front().acknowledgement == 101);

    const auto established = server.receive({0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000, 101,
                                             synAck.front().sequence + 1, camera::infra::TcpFlag::Ack, {}});
    REQUIRE(established.empty());

    const std::string request = "GET /camera.xml HTTP/1.1\r\nHost: camera\r\n\r\n";
    const auto reply = server.receive({0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000, 101,
                                       synAck.front().sequence + 1, camera::infra::TcpFlag::Ack,
                                       std::vector<std::byte>(reinterpret_cast<const std::byte*>(request.data()),
                                                              reinterpret_cast<const std::byte*>(request.data() + request.size()))});
    REQUIRE(reply.size() == 1);
    REQUIRE(reply.front().has(camera::infra::TcpFlag::Ack));
    REQUIRE(reply.front().has(camera::infra::TcpFlag::Psh));
    const std::string response(reinterpret_cast<const char*>(reply.front().payload.data()), reply.front().payload.size());
    REQUIRE(response.find("<RegisterDescription/>") != std::string::npos);

    const auto fin = server.receive({0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000, 101 + static_cast<std::uint32_t>(request.size()),
                                     reply.front().sequence + static_cast<std::uint32_t>(reply.front().payload.size()),
                                     camera::infra::TcpFlag::Ack, {}});
    REQUIRE(fin.size() == 1);
    REQUIRE(fin.front().has(camera::infra::TcpFlag::Fin));
    REQUIRE(fin.front().has(camera::infra::TcpFlag::Ack));
    REQUIRE(server.receive({0xa9fe6d10u, 0xa9fe6d11u, 51000, 8000,
                            101 + static_cast<std::uint32_t>(request.size()), fin.front().sequence + 1,
                            camera::infra::TcpFlag::Ack, {}}).empty());
}

TEST_CASE("Tunnel HTTP server retransmits an unacknowledged SYN-ACK", "[TunnelTcp]") {
    camera::infra::TunnelHttpServer server(0xa9fe6d11u, 8000, "<RegisterDescription/>");
    const auto synAck = server.receive(syn(0xa9fe6d10u, 0xa9fe6d11u, 51000));
    REQUIRE(synAck.size() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    const auto retransmission = server.send_due();
    REQUIRE(retransmission.size() == 1);
    REQUIRE(retransmission.front().has(camera::infra::TcpFlag::Syn));
    REQUIRE(retransmission.front().has(camera::infra::TcpFlag::Ack));
}

TEST_CASE("Tunnel HTTP server ignores an acknowledgement from another endpoint", "[TunnelTcp]") {
    camera::infra::TunnelHttpServer server(0xa9fe6d11u, 8000, "<RegisterDescription/>");
    const auto synAck = server.receive(syn(0xa9fe6d10u, 0xa9fe6d11u, 51000));
    REQUIRE(synAck.size() == 1);

    const auto ignored = server.receive({0xa9fe6d20u, 0xa9fe6d11u, 51001, 8000, 101,
                                         synAck.front().sequence + 1, camera::infra::TcpFlag::Ack, {}});
    REQUIRE(ignored.empty());
}

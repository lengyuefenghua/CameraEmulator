#include "infra/http_xml_server.hpp"
#include "infra/logging.hpp"

#include <asio.hpp>

#include <memory>
#include <sstream>
#include <string_view>

namespace camera::infra {
namespace {

struct ServerState {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor{io};
};

void log(const char* level, const std::string& source, const std::string& message) {
    const auto parsedLevel = std::string_view(level) == "ERROR" ? spdlog::level::err
        : std::string_view(level) == "WARN" ? spdlog::level::warn : spdlog::level::info;
    Logging::write(parsedLevel, "HttpXmlServer", source, message);
}

} // namespace

HttpXmlServer::~HttpXmlServer() { stop(); }

bool HttpXmlServer::start(const std::string& bindIp, std::uint16_t port,
                          std::string cameraId, std::string xml, std::string& error) {
    stop();
    auto state = std::make_unique<ServerState>();
    asio::error_code ec;
    const auto address = asio::ip::make_address_v4(bindIp, ec);
    if (ec) {
        error = "invalid IPv4 interface=" + bindIp + " port=" + std::to_string(port) + " error=" + ec.message();
        log("ERROR", "bind", error);
        return false;
    }
    state->acceptor.open(asio::ip::tcp::v4(), ec);
    if (!ec) state->acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (!ec) state->acceptor.bind({address, port}, ec);
    if (!ec) state->acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        error = "interface=" + bindIp + " port=" + std::to_string(port) + " error=" + ec.message();
        log("ERROR", "bind", error);
        return false;
    }
    bindIp_ = bindIp;
    port_ = port;
    cameraId_ = std::move(cameraId);
    xml_ = std::move(xml);
    serverState_ = state.release();
    running_ = true;
    thread_ = std::thread([this] { serve(); });
    log("INFO", "start", "camera=" + cameraId_ + " interface=" + bindIp_ + " port=" + std::to_string(port_));
    return true;
}

void HttpXmlServer::serve() {
    auto* state = static_cast<ServerState*>(serverState_);
    while (running_) {
        asio::ip::tcp::socket socket(state->io);
        asio::error_code ec;
        state->acceptor.accept(socket, ec);
        if (ec) {
            if (running_) log("ERROR", "accept", "interface=" + bindIp_ + " port=" + std::to_string(port_) + " error=" + ec.message());
            continue;
        }
        if (!running_) break;
        asio::streambuf request;
        asio::read_until(socket, request, "\r\n\r\n", ec);
        if (ec) {
            log("WARN", "request", "camera=" + cameraId_ + " error=" + ec.message());
            continue;
        }
        std::istream input(&request);
        std::string method, path, version;
        input >> method >> path >> version;
        const std::string expected = "/" + cameraId_ + ".xml";
        const bool ok = method == "GET" && (path == expected || path == "/" + cameraId_);
        const std::string body = ok ? xml_ : "Not Found\n";
        std::ostringstream response;
        response << (ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n")
                 << "Content-Type: " << (ok ? "application/xml; charset=utf-8" : "text/plain; charset=utf-8") << "\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Connection: close\r\n\r\n" << body;
        const auto responseText = response.str();
        asio::write(socket, asio::buffer(responseText), ec);
        if (ec) log("ERROR", "response", "camera=" + cameraId_ + " error=" + ec.message());
    }
}

void HttpXmlServer::stop() noexcept {
    if (!serverState_) return;
    running_ = false;
    auto* state = static_cast<ServerState*>(serverState_);
    asio::error_code ec;
    {
        asio::ip::tcp::socket wake(state->io);
        wake.connect({asio::ip::make_address_v4(bindIp_), port_}, ec);
        wake.close(ec);
    }
    state->acceptor.cancel(ec);
    state->acceptor.close(ec);
    if (thread_.joinable()) thread_.join();
    delete state;
    serverState_ = nullptr;
    bindIp_.clear();
    cameraId_.clear();
    port_ = 0;
}

} // namespace camera::infra

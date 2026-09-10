#include "musicbox/net/TcpListener.hpp"

#include <arpa/inet.h>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

// Binds to port 0 (OS-assigned ephemeral port) and reads back the actual port
// via getsockname() through the listener's public fd() accessor, so these
// tests never race other tests or other processes over a fixed port number.
std::uint16_t actualPort(const musicbox::net::TcpListener& listener) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(listener.fd(), reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    return ntohs(addr.sin_port);
}

int rawConnect(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(fd >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    REQUIRE(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    return fd;
}

} // namespace

TEST_CASE("TcpListener::accept returns nullopt when nothing is pending", "[net][tcplistener]") {
    musicbox::net::TcpListener listener("127.0.0.1", 0);
    CHECK_FALSE(listener.accept().has_value());
}

TEST_CASE("TcpListener::accept returns a connected, non-blocking socket once a client connects",
          "[net][tcplistener]") {
    musicbox::net::TcpListener listener("127.0.0.1", 0);
    std::uint16_t port = actualPort(listener);

    int clientFd = rawConnect(port);

    // The listener's socket is level-triggered-readable once the kernel has
    // completed the handshake; give it a brief moment on a loaded machine.
    std::optional<musicbox::net::Socket> accepted;
    for (int attempt = 0; attempt < 100 && !accepted.has_value(); ++attempt) {
        accepted = listener.accept();
        if (!accepted.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    REQUIRE(accepted.has_value());
    CHECK(accepted->valid());

    ::close(clientFd);
}

TEST_CASE("TcpListener throws on an invalid bind address", "[net][tcplistener]") {
    CHECK_THROWS_AS(musicbox::net::TcpListener("not-an-ip-address", 0), std::runtime_error);
}

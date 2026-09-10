#pragma once

// Shared helpers for driving a real musicbox::net::EventLoop from a background
// thread and talking to it with plain POSIX client sockets, per the
// fragmentation/backpressure test contract in docs/networking.md. This header
// is not itself a test translation unit (tests/CMakeLists.txt only GLOBs
// *.cpp), so it's safe to share across integration test files.

#include "musicbox/net/EventLoop.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace musicbox::test {

// Runs a real EventLoop on a background thread for the lifetime of the
// object. Register callbacks via onReadable()/onClosed() *before* calling
// start() so there is a happens-before edge (via std::thread's constructor)
// with the loop thread that will read them -- no locking needed.
class TestServer {
public:
    TestServer() = default;

    ~TestServer() {
        // EventLoop::stop() only flips an atomic<bool>; it does not touch any
        // socket, so calling it from this (non-loop) thread does not violate
        // the "only the loop thread calls send()/recv()" rule in
        // docs/architecture.md #4. The header's stronger wording ("must be
        // marshalled in") is aimed at production code paths that don't exist
        // yet (main.cpp's run() never stops); it's the simplest correct way
        // to tear down a test fixture.
        loop_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    TestServer(const TestServer&) = delete;
    TestServer& operator=(const TestServer&) = delete;

    void onReadable(musicbox::net::ConnectionReadableCallback callback) {
        loop_.onReadable(std::move(callback));
    }

    void onClosed(musicbox::net::ConnectionClosedCallback callback) {
        loop_.onClosed(std::move(callback));
    }

    void start(std::uint16_t port, const std::string& address = "127.0.0.1") {
        loop_.listen(address, port);
        thread_ = std::thread([this] { loop_.run(); });
        // Give the background thread a moment to enter run()/poller_->wait()
        // before the test starts connecting. The listener is already bound
        // and non-blocking-accepting by the time listen() returns, so this is
        // a convenience margin rather than a correctness requirement.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

private:
    musicbox::net::EventLoop loop_;
    std::thread thread_;
};

// Opens a blocking, connected TCP client socket to 127.0.0.1:port. Throws on
// failure. Caller owns the fd and must close() it.
inline int connectClient(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("connectClient: socket() failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("connectClient: inet_pton failed");
    }

    // A handful of retries: the background EventLoop thread may not have
    // finished entering accept-readiness the instant start() returns on a
    // loaded CI machine.
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            return fd;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ::close(fd);
    throw std::runtime_error("connectClient: connect() failed after retries");
}

} // namespace musicbox::test

#include "musicbox/net/TcpConnection.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using musicbox::net::Socket;
using musicbox::net::TcpConnection;

namespace {

// A connected pair of non-blocking UNIX-domain sockets (see SocketTest.cpp's
// identical fixture) -- one end wrapped in the TcpConnection under test, the
// other used directly by the test to drain what it sends.
struct SocketPair {
    Socket connectionSide;
    Socket testSide;

    SocketPair() {
        int fds[2];
        REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        connectionSide = Socket(fds[0]);
        testSide = Socket(fds[1]);
        connectionSide.setNonBlocking();
        testSide.setNonBlocking();
    }
};

// Drains `testSide` until `expectedTotal` bytes have been read or `maxCalls`
// flush/read rounds pass without progress (a bug producing too little data
// would otherwise hang the test instead of failing it).
std::string drain(Socket& testSide, std::size_t expectedTotal) {
    std::string result;
    result.reserve(expectedTotal);
    int stallRounds = 0;
    while (result.size() < expectedTotal && stallRounds < 1000) {
        std::array<std::byte, 8192> chunk{};
        ssize_t n = testSide.recv(std::span<std::byte>(chunk));
        if (n > 0) {
            result.append(reinterpret_cast<const char*>(chunk.data()), static_cast<std::size_t>(n));
            stallRounds = 0;
        } else {
            ++stallRounds;
        }
    }
    return result;
}

} // namespace

TEST_CASE("TcpConnection streaming body is sent via flushOutbound in bounded chunks",
          "[net][tcpconnection]") {
    SocketPair pair;
    TcpConnection connection(std::move(pair.connectionSide));

    // A payload comfortably larger than one internal chunk (64 KiB) so the
    // refill loop must run more than once across several flushOutbound() calls
    // -- this is the scenario that would silently regress into "buffer the
    // whole thing" if someone changed the refill condition incorrectly.
    constexpr std::size_t kPayloadSize = 200 * 1024;
    std::string payload(kPayloadSize, '\0');
    for (std::size_t i = 0; i < kPayloadSize; ++i) {
        payload[i] = static_cast<char>(i % 251);
    }

    std::size_t offset = 0;
    connection.setStreamingBody([&payload, &offset](std::span<std::byte> dst) -> std::size_t {
        std::size_t remaining = payload.size() - offset;
        std::size_t n = std::min(remaining, dst.size());
        std::memcpy(dst.data(), payload.data() + offset, n);
        offset += n;
        return n;
    });

    REQUIRE(connection.hasPendingWrites()); // true even though queueWrite() was never called

    // Repeatedly flush + drain, simulating the writability-driven cycle
    // EventLoop::run() performs, until the stream is exhausted.
    std::string received;
    int rounds = 0;
    while (connection.hasPendingWrites() && rounds < 100) {
        connection.flushOutbound();
        std::array<std::byte, 8192> chunk{};
        ssize_t n;
        while ((n = pair.testSide.recv(std::span<std::byte>(chunk))) > 0) {
            received.append(reinterpret_cast<const char*>(chunk.data()),
                            static_cast<std::size_t>(n));
        }
        ++rounds;
    }

    CHECK_FALSE(connection.hasPendingWrites());
    REQUIRE(received.size() == payload.size());
    CHECK(received == payload);
}

TEST_CASE("TcpConnection streaming body composes with a queued header block",
          "[net][tcpconnection]") {
    SocketPair pair;
    TcpConnection connection(std::move(pair.connectionSide));

    const std::string header = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n";
    const std::string body = "hello";

    connection.queueWrite(std::as_bytes(std::span<const char>(header.data(), header.size())));

    std::size_t offset = 0;
    connection.setStreamingBody([&body, &offset](std::span<std::byte> dst) -> std::size_t {
        if (offset >= body.size())
            return 0;
        std::size_t n = std::min(body.size() - offset, dst.size());
        std::memcpy(dst.data(), body.data() + offset, n);
        offset += n;
        return n;
    });

    std::string received;
    int rounds = 0;
    while (connection.hasPendingWrites() && rounds < 100) {
        connection.flushOutbound();
        std::array<std::byte, 8192> chunk{};
        ssize_t n;
        while ((n = pair.testSide.recv(std::span<std::byte>(chunk))) > 0) {
            received.append(reinterpret_cast<const char*>(chunk.data()),
                            static_cast<std::size_t>(n));
        }
        ++rounds;
    }

    CHECK(received == header + body);
}

TEST_CASE("TcpConnection with no streaming body behaves exactly as before",
          "[net][tcpconnection]") {
    SocketPair pair;
    TcpConnection connection(std::move(pair.connectionSide));

    CHECK_FALSE(connection.hasPendingWrites());

    const std::string message = "plain response, no streaming";
    connection.queueWrite(std::as_bytes(std::span<const char>(message.data(), message.size())));
    CHECK(connection.hasPendingWrites());

    connection.flushOutbound();
    CHECK_FALSE(connection.hasPendingWrites());

    const auto received = drain(pair.testSide, message.size());
    CHECK(received == message);
}

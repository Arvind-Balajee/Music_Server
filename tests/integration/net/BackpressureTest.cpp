#include "TestHarness.hpp"

#include "musicbox/net/TcpConnection.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <span>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Exercises docs/networking.md's backpressure contract: a slow client reading
// a large response in small pieces must eventually receive every byte,
// unmodified, without the server hanging, crashing, or growing its outbound
// Buffer unboundedly waiting for a `send()` that never fully drains in one
// call (see TcpConnection::flushOutbound()'s partial-write contract).
TEST_CASE("EventLoop delivers a large response correctly to a slow client", "[net][integration][backpressure]") {
    constexpr std::uint16_t kPort = 19311;
    constexpr std::size_t kPayloadSize = 256 * 1024; // 256 KiB: several times a typical socket buffer.

    std::string payload(kPayloadSize, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>('A' + (i % 26));
    }

    musicbox::test::TestServer server;
    server.onReadable([&](musicbox::net::TcpConnection& connection) {
        auto& inbound = connection.inboundBuffer();
        inbound.consume(inbound.readableBytes());

        if (!connection.hasPendingWrites()) {
            connection.queueWrite(std::as_bytes(std::span<const char>(payload.data(), payload.size())));
            connection.closeAfterFlush();
        }
    });
    server.start(kPort);

    int fd = musicbox::test::connectClient(kPort);
    const char ping = 'x';
    REQUIRE(::send(fd, &ping, 1, 0) == 1);

    std::string received;
    received.reserve(kPayloadSize);
    std::array<char, 256> chunk{};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    while (received.size() < kPayloadSize) {
        ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (n > 0) {
            received.append(chunk.data(), static_cast<std::size_t>(n));
        } else if (n == 0) {
            break; // peer closed early -- loop exits, size check below fails.
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            FAIL("client recv() failed: " << std::strerror(errno));
        }

        REQUIRE(std::chrono::steady_clock::now() < deadline);
        // Simulate a slow reader: sleep between small reads so the server is
        // forced to hit EAGAIN on send() and retry flushOutbound() across
        // multiple writable events rather than draining in one call.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ::close(fd);

    REQUIRE(received.size() == kPayloadSize);
    CHECK(received == payload);
}

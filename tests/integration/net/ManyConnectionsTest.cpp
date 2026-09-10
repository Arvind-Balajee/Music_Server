#include "TestHarness.hpp"

#include "musicbox/net/TcpConnection.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <span>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

void handleEcho(musicbox::net::TcpConnection& connection) {
    auto& inbound = connection.inboundBuffer();
    inbound.consume(inbound.readableBytes());
    constexpr std::string_view response = "ok";
    connection.queueWrite(std::as_bytes(std::span<const char>(response.data(), response.size())));
    connection.closeAfterFlush();
}

} // namespace

// Exercises docs/networking.md's contract that many connections opened and
// closed rapidly must not cause unbounded memory growth, a stuck loop, or a
// crash. Sequential churn from one client thread.
TEST_CASE("EventLoop handles many connections opened and closed in rapid sequence",
          "[net][integration][manyconnections]") {
    constexpr std::uint16_t kPort = 19312;
    constexpr int kConnections = 300;

    std::atomic<int> readableCount{0};
    std::atomic<int> closedCount{0};

    musicbox::test::TestServer server;
    server.onReadable([&](musicbox::net::TcpConnection& connection) {
        readableCount.fetch_add(1, std::memory_order_relaxed);
        handleEcho(connection);
    });
    server.onClosed([&](musicbox::net::TcpConnection&) { closedCount.fetch_add(1, std::memory_order_relaxed); });
    server.start(kPort);

    for (int i = 0; i < kConnections; ++i) {
        int fd = musicbox::test::connectClient(kPort);
        const char ping = 'p';
        REQUIRE(::send(fd, &ping, 1, 0) == 1);

        std::array<char, 16> buf{};
        ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
        REQUIRE(n == 2);
        CHECK(std::string(buf.data(), static_cast<std::size_t>(n)) == "ok");

        ::close(fd);
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (closedCount.load(std::memory_order_relaxed) < kConnections &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(readableCount.load() == kConnections);
    CHECK(closedCount.load() == kConnections);
}

// Same contract, but with concurrent churn from multiple client threads to
// stress ConnectionManager's fd->connection map and the poller's add/remove
// bookkeeping under contention from the connection-accept side (the EventLoop
// itself is still single-threaded; only the *clients* are concurrent).
TEST_CASE("EventLoop handles concurrent connection churn from multiple client threads",
          "[net][integration][manyconnections]") {
    constexpr std::uint16_t kPort = 19313;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 25;

    musicbox::test::TestServer server;
    server.onReadable([](musicbox::net::TcpConnection& connection) { handleEcho(connection); });
    server.start(kPort);

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < kPerThread; ++i) {
                int fd = musicbox::test::connectClient(kPort);
                const char ping = 'p';
                if (::send(fd, &ping, 1, 0) != 1) {
                    ::close(fd);
                    continue;
                }
                std::array<char, 16> buf{};
                ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
                if (n == 2 && std::string(buf.data(), 2) == "ok") {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
                ::close(fd);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    CHECK(successCount.load() == kThreads * kPerThread);
}

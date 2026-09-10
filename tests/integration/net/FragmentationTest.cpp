#include "TestHarness.hpp"

#include "musicbox/net/TcpConnection.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <sys/socket.h>
#include <unistd.h>

// Exercises docs/networking.md's "Fragmentation & Backpressure Test Contract":
// a request split across 1-, 2-, and N-byte send() calls from a raw client
// socket must be reassembled correctly by EventLoop/TcpConnection/Buffer no
// matter how it was chopped up on the wire.
//
// Each SECTION below runs with exactly one connection alive at a time, so a
// single shared accumulator (rather than one keyed per-fd) is enough --
// deliberately so, since the client's own fd and the server's accepted fd for
// the *same* TCP connection are unrelated integers within this one process
// (two independent fd namespaces from the OS's point of view, both just
// happening to be small ints), so keying by "the fd the test happens to hold"
// would not match what the server-side callback sees.
TEST_CASE("EventLoop reassembles a request split across many small sends", "[net][integration][fragmentation]") {
    constexpr std::uint16_t kPort = 19310;
    const std::string message = "GET /fragmented HTTP/1.1\r\nHost: musicbox.local\r\n\r\n";

    std::mutex mutex;
    std::condition_variable cv;
    std::string accumulated;
    bool done = false;

    musicbox::test::TestServer server;
    server.onReadable([&](musicbox::net::TcpConnection& connection) {
        auto& inbound = connection.inboundBuffer();
        auto view = inbound.readableView();
        std::string chunk(reinterpret_cast<const char*>(view.data()), view.size());
        inbound.consume(view.size());

        bool justFinished = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            accumulated += chunk;
            if (!done && accumulated.size() >= 4 &&
                accumulated.compare(accumulated.size() - 4, 4, "\r\n\r\n") == 0) {
                done = true;
                justFinished = true;
            }
        }
        if (justFinished) {
            cv.notify_all();
        }
    });
    server.start(kPort);

    auto sendInChunksOfSize = [&](int fd, std::size_t chunkSize) {
        std::size_t offset = 0;
        while (offset < message.size()) {
            std::size_t n = std::min(chunkSize, message.size() - offset);
            ssize_t sent = ::send(fd, message.data() + offset, n, 0);
            REQUIRE(sent > 0);
            offset += static_cast<std::size_t>(sent);
            // Small delay so consecutive sends are much more likely to arrive
            // as separate readable events / recv() calls rather than being
            // coalesced by the kernel into one.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };

    auto runCase = [&](std::size_t chunkSize) {
        int fd = musicbox::test::connectClient(kPort);
        sendInChunksOfSize(fd, chunkSize);

        std::unique_lock<std::mutex> lock(mutex);
        bool ok = cv.wait_for(lock, std::chrono::seconds(10), [&] { return done; });
        REQUIRE(ok);
        CHECK(accumulated == message);
        lock.unlock();

        ::close(fd);
    };

    SECTION("1-byte sends") { runCase(1); }
    SECTION("2-byte sends") { runCase(2); }
    SECTION("N-byte sends (7 bytes)") { runCase(7); }
}

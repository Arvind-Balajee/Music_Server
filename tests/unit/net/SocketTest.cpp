#include "musicbox/net/Socket.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// A connected pair of non-blocking UNIX-domain sockets, wrapped in
// musicbox::net::Socket. Using socketpair() rather than real TCP keeps these
// unit tests fast and independent of any port/EventLoop machinery -- Socket
// itself doesn't know or care what address family it's wrapping.
struct SocketPair {
    musicbox::net::Socket a;
    musicbox::net::Socket b;

    SocketPair() {
        int fds[2];
        REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        a = musicbox::net::Socket(fds[0]);
        b = musicbox::net::Socket(fds[1]);
        a.setNonBlocking();
        b.setNonBlocking();
    }
};

} // namespace

TEST_CASE("Socket default-constructed is invalid", "[net][socket]") {
    musicbox::net::Socket socket;
    CHECK_FALSE(socket.valid());
    CHECK(socket.fd() == -1);
}

TEST_CASE("Socket wraps an fd and reports it valid", "[net][socket]") {
    SocketPair pair;
    CHECK(pair.a.valid());
    CHECK(pair.a.fd() >= 0);
}

TEST_CASE("Socket move transfers ownership; moved-from socket is invalid", "[net][socket]") {
    SocketPair pair;
    int originalFd = pair.a.fd();

    musicbox::net::Socket moved(std::move(pair.a));
    CHECK(moved.fd() == originalFd);
    CHECK(moved.valid());
    CHECK_FALSE(pair.a.valid()); // NOLINT(bugprone-use-after-move) -- intentional check
}

TEST_CASE("Socket move-assignment closes any fd it previously owned", "[net][socket]") {
    SocketPair pair;
    int keepFd = pair.b.fd();

    pair.b = std::move(pair.a); // pair.b's original fd should be closed here
    CHECK(pair.b.fd() != keepFd);
    CHECK_FALSE(pair.a.valid());
}

TEST_CASE("Socket::release relinquishes ownership without closing", "[net][socket]") {
    SocketPair pair;
    int fd = pair.a.release();
    CHECK_FALSE(pair.a.valid());
    // We now own `fd` directly; closing it should succeed (it wasn't already
    // closed by Socket's destructor).
    CHECK(::close(fd) == 0);
}

TEST_CASE("Socket non-blocking recv returns -1/EAGAIN when nothing is available", "[net][socket]") {
    SocketPair pair;
    std::array<std::byte, 16> buffer{};
    errno = 0;
    ssize_t n = pair.a.recv(std::span<std::byte>(buffer));
    CHECK(n == -1);
    CHECK((errno == EAGAIN || errno == EWOULDBLOCK));
}

TEST_CASE("Socket send/recv roundtrip transfers bytes", "[net][socket]") {
    SocketPair pair;
    const std::string_view message = "hello musicbox";
    auto sendBytes = std::as_bytes(std::span<const char>(message.data(), message.size()));

    ssize_t sent = pair.a.send(sendBytes);
    REQUIRE(sent == static_cast<ssize_t>(message.size()));

    std::array<std::byte, 64> buffer{};
    ssize_t received = pair.b.recv(std::span<std::byte>(buffer));
    REQUIRE(received == static_cast<ssize_t>(message.size()));
    CHECK(std::memcmp(buffer.data(), message.data(), message.size()) == 0);
}

TEST_CASE("Socket recv returns 0 on orderly peer shutdown", "[net][socket]") {
    SocketPair pair;
    pair.a = musicbox::net::Socket{}; // close a's end (destructor runs on assignment)

    std::array<std::byte, 16> buffer{};
    ssize_t n = pair.b.recv(std::span<std::byte>(buffer));
    CHECK(n == 0);
}

TEST_CASE("Socket setNonBlocking is idempotent", "[net][socket]") {
    SocketPair pair;
    CHECK_NOTHROW(pair.a.setNonBlocking());
    CHECK_NOTHROW(pair.a.setNonBlocking());
}

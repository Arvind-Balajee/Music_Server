#pragma once

#include <cstddef>
#include <span>
#include <sys/types.h>

namespace musicbox::net {

// RAII wrapper around a POSIX file descriptor representing a socket. Move-only:
// exactly one Socket owns a given fd at a time; the destructor closes it if still
// owned. See docs/networking.md.
class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept;
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    // Releases ownership without closing; caller becomes responsible for the fd.
    [[nodiscard]] int release() noexcept;

    // Puts the socket into non-blocking mode (O_NONBLOCK). Idempotent.
    void setNonBlocking();

    // SO_REUSEADDR. Must be set before bind().
    void setReuseAddr(bool enabled);

    // TCP_NODELAY. Disables Nagle's algorithm for low-latency small writes.
    void setNoDelay(bool enabled);

    // One non-blocking recv(). Returns bytes read (>0), 0 on orderly shutdown by
    // the peer, or -1 with errno set to EAGAIN/EWOULDBLOCK/EINTR (not fatal) or a
    // real error (fatal — caller should close the connection).
    ssize_t recv(std::span<std::byte> destination) noexcept;

    // One non-blocking send(). Returns bytes written (>=0, may be less than
    // destination.size() — callers must retry the remainder later), or -1 with
    // errno set as above. Never assumes a full write completes in one call.
    ssize_t send(std::span<const std::byte> data) noexcept;

private:
    void closeIfOwned() noexcept;

    int fd_ = -1;
};

} // namespace musicbox::net

#include "musicbox/net/Socket.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace musicbox::net {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

} // namespace

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::~Socket() { closeIfOwned(); }

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeIfOwned();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

int Socket::release() noexcept {
    int fd = fd_;
    fd_ = -1;
    return fd;
}

void Socket::closeIfOwned() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Socket::setNonBlocking() {
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        throwErrno("fcntl(F_GETFL)");
    }
    if ((flags & O_NONBLOCK) != 0) {
        return; // already non-blocking; idempotent.
    }
    if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        throwErrno("fcntl(F_SETFL, O_NONBLOCK)");
    }
}

void Socket::setReuseAddr(bool enabled) {
    int value = enabled ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
        throwErrno("setsockopt(SO_REUSEADDR)");
    }
}

void Socket::setNoDelay(bool enabled) {
    int value = enabled ? 1 : 0;
    if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) < 0) {
        throwErrno("setsockopt(TCP_NODELAY)");
    }
}

ssize_t Socket::recv(std::span<std::byte> destination) noexcept {
    return ::recv(fd_, destination.data(), destination.size(), 0);
}

ssize_t Socket::send(std::span<const std::byte> data) noexcept {
#if defined(__linux__)
    // Avoid SIGPIPE on Linux when the peer has already closed its read side;
    // EventLoop additionally ignores SIGPIPE process-wide (see EventLoop.cpp)
    // for platforms without MSG_NOSIGNAL (e.g. macOS/BSD).
    return ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
#else
    return ::send(fd_, data.data(), data.size(), 0);
#endif
}

} // namespace musicbox::net

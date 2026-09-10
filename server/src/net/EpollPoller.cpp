#include "EpollPoller.hpp"

#if defined(__linux__)

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

namespace musicbox::net {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

std::uint32_t eventsFor(bool wantRead, bool wantWrite) {
    std::uint32_t events = 0;
    if (wantRead) {
        events |= static_cast<std::uint32_t>(EPOLLIN);
    }
    if (wantWrite) {
        events |= static_cast<std::uint32_t>(EPOLLOUT);
    }
    return events;
}

} // namespace

EpollPoller::EpollPoller() {
    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        throwErrno("epoll_create1");
    }
}

EpollPoller::~EpollPoller() {
    if (epollFd_ >= 0) {
        ::close(epollFd_);
    }
}

void EpollPoller::add(int fd, bool wantRead, bool wantWrite) {
    epoll_event ev{};
    ev.events = eventsFor(wantRead, wantWrite);
    ev.data.fd = fd;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throwErrno("epoll_ctl(ADD)");
    }
}

void EpollPoller::modify(int fd, bool wantRead, bool wantWrite) {
    epoll_event ev{};
    ev.events = eventsFor(wantRead, wantWrite);
    ev.data.fd = fd;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        throwErrno("epoll_ctl(MOD)");
    }
}

void EpollPoller::remove(int fd) {
    // Ignore errors: the fd may already have been closed (which implicitly
    // removes it from the epoll set), so ENOENT/EBADF here are not fatal.
    ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
}

std::vector<PollEvent> EpollPoller::wait(int timeoutMs) {
    std::array<epoll_event, 256> raw{};
    int n = ::epoll_wait(epollFd_, raw.data(), static_cast<int>(raw.size()), timeoutMs);
    if (n < 0) {
        if (errno == EINTR) {
            return {};
        }
        throwErrno("epoll_wait");
    }

    std::vector<PollEvent> result;
    result.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        PollEvent pe;
        pe.fd = raw[static_cast<std::size_t>(i)].data.fd;
        std::uint32_t flags = raw[static_cast<std::size_t>(i)].events;
        pe.readable = (flags & (EPOLLIN | EPOLLPRI)) != 0;
        pe.writable = (flags & EPOLLOUT) != 0;
        pe.error = (flags & EPOLLERR) != 0;
        pe.hangup = (flags & (EPOLLHUP | EPOLLRDHUP)) != 0;
        result.push_back(pe);
    }
    return result;
}

} // namespace musicbox::net

#endif // __linux__

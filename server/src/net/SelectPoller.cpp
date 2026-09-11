#include "SelectPoller.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

namespace musicbox::net {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

} // namespace

void SelectPoller::add(int fd, bool wantRead, bool wantWrite) {
    interest_[fd] = {wantRead, wantWrite};
}

void SelectPoller::modify(int fd, bool wantRead, bool wantWrite) {
    interest_[fd] = {wantRead, wantWrite};
}

void SelectPoller::remove(int fd) {
    interest_.erase(fd);
}

std::vector<PollEvent> SelectPoller::wait(int timeoutMs) {
    fd_set readSet;
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);

    int maxFd = -1;
    for (const auto& [fd, in] : interest_) {
        if (in.read) {
            FD_SET(fd, &readSet);
        }
        if (in.write) {
            FD_SET(fd, &writeSet);
        }
        FD_SET(fd, &errorSet);
        maxFd = std::max(maxFd, fd);
    }

    struct timeval tv{};
    struct timeval* tvPtr = nullptr;
    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        tvPtr = &tv;
    }

    int n = ::select(maxFd + 1, &readSet, &writeSet, &errorSet, tvPtr);
    if (n < 0) {
        if (errno == EINTR) {
            return {};
        }
        throwErrno("select");
    }

    std::vector<PollEvent> result;
    if (n == 0) {
        return result; // timeout, nothing ready
    }

    result.reserve(static_cast<std::size_t>(n));
    for (const auto& [fd, in] : interest_) {
        bool r = in.read && FD_ISSET(fd, &readSet);
        bool w = in.write && FD_ISSET(fd, &writeSet);
        bool e = FD_ISSET(fd, &errorSet);
        if (r || w || e) {
            PollEvent pe;
            pe.fd = fd;
            pe.readable = r;
            pe.writable = w;
            pe.error = e;
            result.push_back(pe);
        }
    }
    return result;
}

} // namespace musicbox::net

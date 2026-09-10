#include "KqueuePoller.hpp"

#if defined(MUSICBOX_HAS_KQUEUE)

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

namespace musicbox::net {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

} // namespace

KqueuePoller::KqueuePoller() {
    kq_ = ::kqueue();
    if (kq_ < 0) {
        throwErrno("kqueue");
    }
}

KqueuePoller::~KqueuePoller() {
    if (kq_ >= 0) {
        ::close(kq_);
    }
}

void KqueuePoller::updateInterest(int fd, Interest previous, Interest wanted) {
    std::array<struct kevent, 2> changes{};
    int count = 0;

    if (wanted.read != previous.read) {
        EV_SET(&changes[static_cast<std::size_t>(count)], fd, EVFILT_READ,
               wanted.read ? EV_ADD : EV_DELETE, 0, 0, nullptr);
        ++count;
    }
    if (wanted.write != previous.write) {
        EV_SET(&changes[static_cast<std::size_t>(count)], fd, EVFILT_WRITE,
               wanted.write ? EV_ADD : EV_DELETE, 0, 0, nullptr);
        ++count;
    }

    if (count > 0) {
        // Register-only call (nevents == 0): best-effort. A DELETE racing a
        // fd that the kernel already dropped (e.g. it was just closed) can
        // return ENOENT here; that's not actionable by the caller since
        // `remove()` is also called unconditionally from EventLoop's close
        // path, so we don't throw on this path.
        ::kevent(kq_, changes.data(), count, nullptr, 0, nullptr);
    }
}

void KqueuePoller::add(int fd, bool wantRead, bool wantWrite) {
    Interest wanted{wantRead, wantWrite};
    updateInterest(fd, Interest{}, wanted);
    interest_[fd] = wanted;
}

void KqueuePoller::modify(int fd, bool wantRead, bool wantWrite) {
    Interest wanted{wantRead, wantWrite};
    Interest previous = interest_[fd]; // default-constructed if not present
    updateInterest(fd, previous, wanted);
    interest_[fd] = wanted;
}

void KqueuePoller::remove(int fd) {
    auto it = interest_.find(fd);
    if (it == interest_.end()) {
        return;
    }
    updateInterest(fd, it->second, Interest{});
    interest_.erase(it);
}

std::vector<PollEvent> KqueuePoller::wait(int timeoutMs) {
    std::array<struct kevent, 256> raw{};

    struct timespec ts {};
    struct timespec* tsPtr = nullptr;
    if (timeoutMs >= 0) {
        ts.tv_sec = timeoutMs / 1000;
        ts.tv_nsec = (timeoutMs % 1000) * 1000000L;
        tsPtr = &ts;
    }

    int n = ::kevent(kq_, nullptr, 0, raw.data(), static_cast<int>(raw.size()), tsPtr);
    if (n < 0) {
        if (errno == EINTR) {
            return {};
        }
        throwErrno("kevent(wait)");
    }

    // A single fd can appear twice in one batch (once for EVFILT_READ, once
    // for EVFILT_WRITE) — merge into one PollEvent per fd, matching the
    // Poller contract's shape (one entry per ready fd).
    std::unordered_map<int, PollEvent> merged;
    merged.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const struct kevent& ev = raw[static_cast<std::size_t>(i)];
        int fd = static_cast<int>(ev.ident);
        PollEvent& pe = merged[fd];
        pe.fd = fd;
        if (ev.filter == EVFILT_READ) {
            pe.readable = true;
        } else if (ev.filter == EVFILT_WRITE) {
            pe.writable = true;
        }
        if ((ev.flags & EV_EOF) != 0) {
            pe.hangup = true;
        }
        if ((ev.flags & EV_ERROR) != 0) {
            pe.error = true;
        }
    }

    std::vector<PollEvent> result;
    result.reserve(merged.size());
    for (auto& [fd, pe] : merged) {
        result.push_back(pe);
    }
    return result;
}

} // namespace musicbox::net

#endif // MUSICBOX_HAS_KQUEUE

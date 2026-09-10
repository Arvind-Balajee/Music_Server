#pragma once

// kqueue-backed Poller for macOS/BSD. Internal to server/src/net/ — not a
// public header; callers should only ever hold a `Poller*`/`unique_ptr<Poller>`
// obtained from makePlatformPoller() (see PollerFactory.cpp).
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define MUSICBOX_HAS_KQUEUE 1
#endif

#if defined(MUSICBOX_HAS_KQUEUE)

#include "musicbox/net/Poller.hpp"

#include <unordered_map>

namespace musicbox::net {

class KqueuePoller final : public Poller {
public:
    KqueuePoller();
    ~KqueuePoller() override;

    KqueuePoller(const KqueuePoller&) = delete;
    KqueuePoller& operator=(const KqueuePoller&) = delete;

    void add(int fd, bool wantRead, bool wantWrite) override;
    void modify(int fd, bool wantRead, bool wantWrite) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeoutMs) override;

private:
    struct Interest {
        bool read = false;
        bool write = false;
    };

    // Applies the delta between the previously-registered interest and the
    // newly-requested one as EV_ADD/EV_DELETE changes in a single kevent() call.
    void updateInterest(int fd, Interest previous, Interest wanted);

    int kq_ = -1;
    std::unordered_map<int, Interest> interest_;
};

} // namespace musicbox::net

#endif // MUSICBOX_HAS_KQUEUE

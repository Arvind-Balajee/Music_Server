#pragma once

// Linux epoll-backed Poller. Internal to server/src/net/ — not a public header;
// callers should only ever hold a `Poller*`/`unique_ptr<Poller>` obtained from
// makePlatformPoller() (see PollerFactory.cpp).
#if defined(__linux__)

#include "musicbox/net/Poller.hpp"

namespace musicbox::net {

class EpollPoller final : public Poller {
public:
    EpollPoller();
    ~EpollPoller() override;

    EpollPoller(const EpollPoller&) = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    void add(int fd, bool wantRead, bool wantWrite) override;
    void modify(int fd, bool wantRead, bool wantWrite) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeoutMs) override;

private:
    int epollFd_ = -1;
};

} // namespace musicbox::net

#endif // __linux__

#pragma once

// Portable select()-based Poller, used only when neither epoll nor kqueue is
// available. Internal to server/src/net/ — not a public header; callers should
// only ever hold a `Poller*`/`unique_ptr<Poller>` obtained from
// makePlatformPoller() (see PollerFactory.cpp).
//
// Limitation: select() can only observe fds below FD_SETSIZE (typically 1024)
// and rebuilds its fd_sets from scratch every call, which is O(registered fds)
// per wait() — acceptable for a fallback path, not for the primary platforms.

#include "musicbox/net/Poller.hpp"

#include <unordered_map>

namespace musicbox::net {

class SelectPoller final : public Poller {
public:
    void add(int fd, bool wantRead, bool wantWrite) override;
    void modify(int fd, bool wantRead, bool wantWrite) override;
    void remove(int fd) override;
    std::vector<PollEvent> wait(int timeoutMs) override;

private:
    struct Interest {
        bool read = false;
        bool write = false;
    };

    std::unordered_map<int, Interest> interest_;
};

} // namespace musicbox::net

#pragma once

#include <memory>
#include <vector>

namespace musicbox::net {

struct PollEvent {
    int fd = -1;
    bool readable = false;
    bool writable = false;
    bool error = false;
    bool hangup = false;
};

// Platform-agnostic readiness polling. EventLoop depends only on this interface;
// no epoll/kqueue-specific code exists outside the concrete implementations.
// See docs/networking.md and docs/adr/0003-use-epoll-linux.md.
class Poller {
public:
    virtual ~Poller() = default;

    virtual void add(int fd, bool wantRead, bool wantWrite) = 0;
    virtual void modify(int fd, bool wantRead, bool wantWrite) = 0;
    virtual void remove(int fd) = 0;

    // Blocks for up to timeoutMs (negative = block indefinitely) waiting for
    // readiness on any registered fd. Returns the events that are ready; may
    // return an empty vector on timeout.
    virtual std::vector<PollEvent> wait(int timeoutMs) = 0;
};

// Selects the best available implementation for the current platform:
// EpollPoller on Linux, KqueuePoller on macOS/BSD, SelectPoller otherwise.
std::unique_ptr<Poller> makePlatformPoller();

} // namespace musicbox::net

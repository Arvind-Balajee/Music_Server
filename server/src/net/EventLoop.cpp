#include "musicbox/net/EventLoop.hpp"

#include <csignal>
#include <utility>

namespace musicbox::net {

namespace {

// send() to a socket whose peer already closed its read side raises SIGPIPE
// on platforms without MSG_NOSIGNAL (macOS/BSD); Socket::send() uses
// MSG_NOSIGNAL on Linux, but ignoring SIGPIPE process-wide is the simplest
// portable belt-and-suspenders fix (the resulting EPIPE is still surfaced to
// TcpConnection as a normal -1/errno return either way).
void ignoreSigPipeOnce() {
    static const bool ignored = [] {
        std::signal(SIGPIPE, SIG_IGN);
        return true;
    }();
    (void)ignored;
}

} // namespace

EventLoop::EventLoop() : poller_(makePlatformPoller()) {
    ignoreSigPipeOnce();
}

EventLoop::~EventLoop() = default;

void EventLoop::listen(const std::string& address, std::uint16_t port) {
    auto listener = std::make_unique<TcpListener>(address, port);
    poller_->add(listener->fd(), /*wantRead=*/true, /*wantWrite=*/false);
    listeners_.push_back(std::move(listener));
}

void EventLoop::onReadable(ConnectionReadableCallback callback) {
    onReadable_ = std::move(callback);
}

void EventLoop::onClosed(ConnectionClosedCallback callback) {
    onClosed_ = std::move(callback);
}

void EventLoop::stop() noexcept {
    running_.store(false, std::memory_order_relaxed);
}

void EventLoop::run() {
    running_.store(true, std::memory_order_relaxed);

    // Accepts every pending connection on `listenerFd` (looping until the
    // TcpListener reports EAGAIN, per docs/networking.md step 1), registering
    // each one with the poller for read-interest only and handing ownership to
    // ConnectionManager (connections_).
    auto acceptNewConnections = [this](int listenerFd) {
        TcpListener* listener = nullptr;
        for (const auto& candidate : listeners_) {
            if (candidate->fd() == listenerFd) {
                listener = candidate.get();
                break;
            }
        }
        if (listener == nullptr) {
            return;
        }

        while (true) {
            std::optional<Socket> accepted = listener->accept();
            if (!accepted.has_value()) {
                break;
            }
            int fd = accepted->fd();
            auto connection = std::make_unique<TcpConnection>(std::move(*accepted));
            poller_->add(fd, /*wantRead=*/true, /*wantWrite=*/false);
            connections_.emplace(fd, std::move(connection));
        }
    };

    // Unregisters and destroys the connection for `fd`. The TcpConnection (and
    // therefore its Socket) outlives the onClosed_ callback so handlers can
    // still inspect it, per the header's documented contract, then is torn
    // down immediately after (Socket's RAII dtor closes the fd).
    auto closeConnection = [this](int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return;
        }
        poller_->remove(fd);
        if (onClosed_) {
            onClosed_(*it->second);
        }
        connections_.erase(it);
    };

    while (running_.load(std::memory_order_relaxed)) {
        // A bounded (not infinite) timeout so a stop() request from within a
        // callback is never delayed for more than this long, and so this loop
        // can periodically re-check `running_`.
        std::vector<PollEvent> events = poller_->wait(1000);

        for (const PollEvent& event : events) {
            bool isListenerFd = false;
            for (const auto& listener : listeners_) {
                if (listener->fd() == event.fd) {
                    isListenerFd = true;
                    break;
                }
            }
            if (isListenerFd) {
                acceptNewConnections(event.fd);
                continue;
            }

            auto it = connections_.find(event.fd);
            if (it == connections_.end()) {
                // Stale event for an fd we already closed earlier in this same
                // batch (e.g. it appeared once for read and once for write) —
                // nothing to do.
                continue;
            }
            TcpConnection& connection = *it->second;

            bool shouldClose = event.error;

            if (!shouldClose && (event.readable || event.hangup)) {
                // Level-triggered readiness means EPOLLHUP/EV_EOF can arrive
                // alongside (or instead of) a readable flag while there is
                // still buffered data to drain, so always attempt one recv()
                // in either case; a definitive EOF is recv() returning 0, not
                // the hangup flag itself.
                ssize_t n = connection.readInto();
                if (n == 0) {
                    shouldClose = true;
                } else if (n < 0) {
                    if (connection.hasError()) {
                        shouldClose = true;
                    } else if (event.hangup) {
                        // EAGAIN on an already-hung-up fd: no more data will
                        // ever arrive.
                        shouldClose = true;
                    }
                } else if (onReadable_) {
                    onReadable_(connection);
                    if (connection.hasPendingWrites()) {
                        // Opportunistic write: try to drain immediately rather
                        // than always waiting for a separate writable event.
                        connection.flushOutbound();
                        shouldClose = connection.hasError();
                    }
                }
            }

            if (!shouldClose && event.writable && connection.hasPendingWrites()) {
                connection.flushOutbound();
                shouldClose = connection.hasError();
            }

            if (!shouldClose && connection.wantsCloseAfterFlush() &&
                !connection.hasPendingWrites()) {
                shouldClose = true;
            }

            if (shouldClose) {
                closeConnection(event.fd);
            } else {
                poller_->modify(event.fd, /*wantRead=*/true, connection.hasPendingWrites());
            }
        }
    }
}

} // namespace musicbox::net

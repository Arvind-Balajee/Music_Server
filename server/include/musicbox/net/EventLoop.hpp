#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "musicbox/net/Poller.hpp"
#include "musicbox/net/TcpConnection.hpp"
#include "musicbox/net/TcpListener.hpp"

namespace musicbox::net {

// Invoked when a connection has newly-readable bytes buffered in
// TcpConnection::inboundBuffer(). The callback consumes what it can from the
// buffer (see docs/http.md — the parser handles partial/fragmented messages).
using ConnectionReadableCallback = std::function<void(TcpConnection&)>;

// Invoked when a connection is closed (peer disconnect, error, or hangup) so the
// HTTP layer can release any per-connection state. `connection` is still valid
// during the call but will be destroyed immediately after.
using ConnectionClosedCallback = std::function<void(TcpConnection&)>;

// The single-threaded reactor: owns one Poller, one or more TcpListeners, and all
// live TcpConnections (via an internal ConnectionManager keyed by fd). Exactly one
// thread calls run(); see docs/architecture.md §4 for the full threading model.
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Binds and starts listening; connections accepted here are dispatched to the
    // callbacks registered below. May be called more than once to listen on
    // multiple addresses/ports.
    void listen(const std::string& address, std::uint16_t port);

    void onReadable(ConnectionReadableCallback callback);
    void onClosed(ConnectionClosedCallback callback);

    // Runs until stop() is called from within a callback (this is a single-
    // threaded loop; stop() from another thread must be marshalled in, e.g. via
    // a self-pipe, not called directly).
    void run();
    void stop() noexcept;

private:
    std::unique_ptr<Poller> poller_;
    std::vector<std::unique_ptr<TcpListener>> listeners_;
    std::unordered_map<int, std::unique_ptr<TcpConnection>> connections_;

    ConnectionReadableCallback onReadable_;
    ConnectionClosedCallback onClosed_;

    std::atomic<bool> running_{false};
};

} // namespace musicbox::net

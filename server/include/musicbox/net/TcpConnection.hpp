#pragma once

#include <cstddef>
#include <span>

#include "musicbox/net/Buffer.hpp"
#include "musicbox/net/Socket.hpp"

namespace musicbox::net {

// One accepted client connection. Protocol-agnostic: it knows nothing about HTTP.
// The HTTP layer (docs/http.md) is driven by inboundBuffer() and queueWrite();
// only the EventLoop thread ever calls readInto()/flushOutbound() on a given
// instance, so no internal locking is required here (see docs/architecture.md §4
// for how worker-thread results get back onto this thread safely).
class TcpConnection {
public:
    explicit TcpConnection(Socket socket);

    [[nodiscard]] int fd() const noexcept { return socket_.fd(); }

    // One non-blocking recv() into the inbound buffer. Returns bytes read, 0 on
    // orderly peer shutdown, -1 on EAGAIN (not an error — just nothing to read
    // right now). A real error is reported via hasError().
    ssize_t readInto();

    [[nodiscard]] Buffer& inboundBuffer() noexcept { return inbound_; }

    // Appends bytes to the outbound queue; does not necessarily send them yet.
    void queueWrite(std::span<const std::byte> data);

    // Attempts to send as much of the outbound buffer as the kernel will accept
    // right now. Must be called again (when the fd becomes writable) if it
    // doesn't fully drain the queue — never assumes one send() flushes everything.
    void flushOutbound();

    [[nodiscard]] bool hasPendingWrites() const noexcept { return !outbound_.empty(); }
    [[nodiscard]] bool hasError() const noexcept { return error_; }

    // Additive (see docs/adr/0007-tcpconnection-close-after-flush.md): tells the EventLoop to
    // close this connection once the outbound buffer is fully drained, instead
    // of requiring a second round trip through the readable/writable callbacks.
    // Used for HTTP/1.0-style or "Connection: close" responses.
    void closeAfterFlush() noexcept { closeAfterFlush_ = true; }
    [[nodiscard]] bool wantsCloseAfterFlush() const noexcept { return closeAfterFlush_; }

private:
    Socket socket_;
    Buffer inbound_;
    Buffer outbound_;
    bool error_ = false;
    bool closeAfterFlush_ = false;
};

} // namespace musicbox::net

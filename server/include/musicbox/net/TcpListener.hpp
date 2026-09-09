#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "musicbox/net/Socket.hpp"

namespace musicbox::net {

// Owns a bound, listening, non-blocking socket.
class TcpListener {
public:
    // Binds and listens on address:port. Throws std::runtime_error on failure
    // (bind/listen errors are not recoverable at startup).
    TcpListener(const std::string& address, std::uint16_t port, int backlog = 128);

    [[nodiscard]] int fd() const noexcept { return socket_.fd(); }

    // Non-blocking accept. Returns std::nullopt if no connection is currently
    // pending (EAGAIN/EWOULDBLOCK) — this is the normal/expected case, not an
    // error. Callers should loop calling accept() until nullopt is returned.
    [[nodiscard]] std::optional<Socket> accept();

private:
    Socket socket_;
};

} // namespace musicbox::net

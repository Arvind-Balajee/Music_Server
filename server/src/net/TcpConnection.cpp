#include "musicbox/net/TcpConnection.hpp"

#include <array>
#include <cerrno>

namespace musicbox::net {

namespace {
constexpr std::size_t kReadChunkSize = 64 * 1024;
}

TcpConnection::TcpConnection(Socket socket) : socket_(std::move(socket)) {}

ssize_t TcpConnection::readInto() {
    std::array<std::byte, kReadChunkSize> chunk{};
    ssize_t n = socket_.recv(std::span<std::byte>(chunk));
    if (n > 0) {
        inbound_.append(std::span<const std::byte>(chunk.data(), static_cast<std::size_t>(n)));
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            error_ = true;
        }
    }
    // n == 0 -> orderly peer shutdown; nothing to append, caller closes.
    return n;
}

void TcpConnection::queueWrite(std::span<const std::byte> data) { outbound_.append(data); }

void TcpConnection::flushOutbound() {
    if (outbound_.empty()) {
        return;
    }

    ssize_t n = socket_.send(outbound_.readableView());
    if (n > 0) {
        outbound_.consume(static_cast<std::size_t>(n));
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            error_ = true;
        }
    }
    // A partial send (0 <= n < readableView().size()) is expected and fine: the
    // caller is required to invoke flushOutbound() again once the fd is next
    // writable (see docs/networking.md) — we never assume one send() drains it.
}

} // namespace musicbox::net

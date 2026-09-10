#include "musicbox/net/TcpListener.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

namespace musicbox::net {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

} // namespace

TcpListener::TcpListener(const std::string& address, std::uint16_t port, int backlog) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throwErrno("socket");
    }
    socket_ = Socket(fd);
    socket_.setReuseAddr(true);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("TcpListener: invalid bind address '" + address + "'");
    }

    if (::bind(socket_.fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throwErrno("bind");
    }
    if (::listen(socket_.fd(), backlog) < 0) {
        throwErrno("listen");
    }

    // Non-blocking last: accept()/EventLoop rely on EAGAIN semantics, and we
    // want any setup failure above to surface as a blocking-mode errno message.
    socket_.setNonBlocking();
}

std::optional<Socket> TcpListener::accept() {
    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    int clientFd = ::accept(socket_.fd(), reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (clientFd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // No pending connection right now (or accept() was interrupted) —
            // both are the normal/expected case, not an error.
            return std::nullopt;
        }
        throwErrno("accept");
    }

    Socket client(clientFd);
    client.setNonBlocking();
    client.setNoDelay(true);
    return client;
}

} // namespace musicbox::net

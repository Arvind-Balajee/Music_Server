#include "musicbox/sync/HttpClient.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

namespace musicbox::sync {

namespace {

// RAII wrapper so every early-return path still closes the fd (Plan.md §4:
// RAII for all system resources).
class ScopedSocket {
public:
    explicit ScopedSocket(int fd) : fd_(fd) {}
    ~ScopedSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

private:
    int fd_;
};

// Loops over send() until all bytes are written or a real error occurs.
// Handles partial writes and EINTR (Plan.md §4 item 15).
bool sendAll(int fd, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        const ssize_t n = ::send(fd, data + sent, size - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// Reads exactly the HTTP response head + Content-Length-delimited body.
// Handles partial reads and EINTR (Plan.md §4 item 14). No chunked
// transfer-encoding support -- see HttpClient.hpp for why that's acceptable
// here.
HttpClientResponse readResponse(int fd) {
    HttpClientResponse response;
    response.connected = true;

    std::string raw;
    std::array<char, 8192> chunk{};
    std::size_t headerEnd = std::string::npos;

    // Read until we've seen the end of the headers (\r\n\r\n).
    while (headerEnd == std::string::npos) {
        const ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            response.connected = false;
            response.error = "recv() failed while reading headers";
            return response;
        }
        if (n == 0) {
            response.connected = false;
            response.error = "connection closed before headers completed";
            return response;
        }
        raw.append(chunk.data(), static_cast<std::size_t>(n));
        headerEnd = raw.find("\r\n\r\n");
    }

    const std::string headBlock = raw.substr(0, headerEnd);
    std::string bodySoFar = raw.substr(headerEnd + 4);

    std::istringstream headStream(headBlock);
    std::string statusLine;
    std::getline(headStream, statusLine);
    {
        std::istringstream statusStream(statusLine);
        std::string httpVersion;
        statusStream >> httpVersion >> response.statusCode;
    }

    long contentLength = -1;
    std::string headerLine;
    while (std::getline(headStream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        const std::string lowerPrefix = "content-length:";
        if (headerLine.size() >= lowerPrefix.size()) {
            std::string prefixCandidate = headerLine.substr(0, lowerPrefix.size());
            for (auto& c : prefixCandidate) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (prefixCandidate == lowerPrefix) {
                contentLength = std::strtol(headerLine.c_str() + lowerPrefix.size(), nullptr, 10);
            }
        }
    }

    if (contentLength < 0) {
        // No Content-Length: read until the peer closes the connection
        // (acceptable for a "Connection: close" request as sent below).
        while (true) {
            const ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (n == 0) {
                break;
            }
            bodySoFar.append(chunk.data(), static_cast<std::size_t>(n));
        }
    } else {
        while (bodySoFar.size() < static_cast<std::size_t>(contentLength)) {
            const ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                response.connected = false;
                response.error = "recv() failed while reading body";
                return response;
            }
            if (n == 0) {
                break; // Peer closed early; return whatever body we have.
            }
            bodySoFar.append(chunk.data(), static_cast<std::size_t>(n));
        }
    }

    response.body = std::move(bodySoFar);
    return response;
}

// Resolves host:port and connects, trying each resolved address in turn.
// Returns a valid fd on success, or -1 with `error` populated on failure.
int connectTo(const std::string& host, int port, std::string& error) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* resolved = nullptr;
    const std::string portStr = std::to_string(port);
    const int gaiResult = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &resolved);
    if (gaiResult != 0 || resolved == nullptr) {
        error = std::string("DNS/address resolution failed for ") + host + ": " + gai_strerror(gaiResult);
        return -1;
    }

    int fd = -1;
    for (addrinfo* candidate = resolved; candidate != nullptr; candidate = candidate->ai_next) {
        fd = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (::connect(fd, candidate->ai_addr, candidate->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(resolved);

    if (fd < 0) {
        error = "could not connect to " + host + ":" + portStr;
    }
    return fd;
}

} // namespace

SimpleHttpClient::SimpleHttpClient(std::string hostAndMaybePort, int defaultPort) : port_(defaultPort) {
    const auto colonPos = hostAndMaybePort.find(':');
    if (colonPos != std::string::npos) {
        host_ = hostAndMaybePort.substr(0, colonPos);
        port_ = std::atoi(hostAndMaybePort.c_str() + colonPos + 1);
        if (port_ <= 0) {
            port_ = defaultPort;
        }
    } else {
        host_ = std::move(hostAndMaybePort);
    }
}

HttpClientResponse SimpleHttpClient::sendRequest(const std::string& requestHead, const char* bodyData,
                                                  std::size_t bodySize) const {
    HttpClientResponse response;

    std::string connectError;
    const int fd = connectTo(host_, port_, connectError);
    if (fd < 0) {
        response.connected = false;
        response.error = connectError;
        return response;
    }

    ScopedSocket socket(fd);

    if (!sendAll(socket.get(), requestHead.data(), requestHead.size())) {
        response.connected = false;
        response.error = "send() failed while writing request headers";
        return response;
    }
    if (bodySize > 0 && !sendAll(socket.get(), bodyData, bodySize)) {
        response.connected = false;
        response.error = "send() failed while writing request body";
        return response;
    }

    return readResponse(socket.get());
}

HttpClientResponse SimpleHttpClient::get(const std::string& path) const {
    std::ostringstream head;
    head << "GET " << path << " HTTP/1.1\r\n"
         << "Host: " << host_ << "\r\n"
         << "Accept: application/json\r\n"
         << "Connection: close\r\n"
         << "\r\n";
    return sendRequest(head.str(), nullptr, 0);
}

HttpClientResponse SimpleHttpClient::postFile(const std::string& path, const std::string& relativePath,
                                               const std::string& absoluteFilePath) const {
    std::error_code sizeEc;
    const auto fileSize = std::filesystem::file_size(absoluteFilePath, sizeEc);
    if (sizeEc) {
        HttpClientResponse response;
        response.connected = false;
        response.error = "could not stat file: " + absoluteFilePath;
        return response;
    }

    std::ifstream file(absoluteFilePath, std::ios::binary);
    if (!file) {
        HttpClientResponse response;
        response.connected = false;
        response.error = "could not open file: " + absoluteFilePath;
        return response;
    }

    std::ostringstream head;
    head << "POST " << path << " HTTP/1.1\r\n"
         << "Host: " << host_ << "\r\n"
         << "X-Relative-Path: " << relativePath << "\r\n"
         << "Content-Type: application/octet-stream\r\n"
         << "Content-Length: " << fileSize << "\r\n"
         << "Connection: close\r\n"
         << "\r\n";

    // Connect and stream the file in bounded chunks rather than reading the
    // whole thing into memory first (Plan.md §4 item 16 -- applies to the sync
    // client's uploads just as much as the server's downloads).
    std::string connectError;
    const int fd = connectTo(host_, port_, connectError);
    if (fd < 0) {
        HttpClientResponse response;
        response.connected = false;
        response.error = connectError;
        return response;
    }

    ScopedSocket socket(fd);
    const std::string headStr = head.str();
    if (!sendAll(socket.get(), headStr.data(), headStr.size())) {
        HttpClientResponse response;
        response.connected = false;
        response.error = "send() failed while writing request headers";
        return response;
    }

    std::array<char, 65536> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = file.gcount();
        if (got <= 0) {
            break;
        }
        if (!sendAll(socket.get(), buffer.data(), static_cast<std::size_t>(got))) {
            HttpClientResponse response;
            response.connected = false;
            response.error = "send() failed while streaming file body";
            return response;
        }
    }

    return readResponse(socket.get());
}

} // namespace musicbox::sync

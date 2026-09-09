#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace musicbox::http {

// Abstraction for a response body that does not need to be fully materialized in
// memory. Implementations (e.g. FileStreamResponseBody) read incrementally from
// their underlying source (a file descriptor via pread()) on demand, so the
// connection layer can stream arbitrarily large files in bounded memory.
//
// Ownership: a ResponseBody is owned exclusively by the HttpResponse that carries
// it (via std::unique_ptr) for the lifetime of sending that one response.
class ResponseBody {
public:
    virtual ~ResponseBody() = default;

    // Writes up to destination.size() bytes into destination, starting from
    // wherever the previous call left off. Returns the number of bytes actually
    // written; returns 0 to signal end-of-stream. Must not block indefinitely —
    // implementations back onto non-blocking I/O (pread on a regular file never
    // blocks meaningfully; this contract matters if a future ResponseBody wraps
    // something that can).
    virtual std::size_t read(std::span<std::byte> destination) = 0;

    // Total bytes remaining to be read, if known up front (used to set
    // Content-Length). std::nullopt if the length isn't known in advance
    // (e.g. chunked transfer — not required for the MVP, see docs/http.md).
    [[nodiscard]] virtual std::optional<std::uint64_t> remaining() const = 0;
};

} // namespace musicbox::http

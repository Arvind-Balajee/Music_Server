#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "musicbox/http/ResponseBody.hpp"

namespace musicbox::streaming {

// Opens a resolved absolute path and exposes its size plus a ResponseBody window
// over [offset, offset+length). This is the zero-copy abstraction referenced in
// docs/architecture.md §7 / docs/http.md: the default implementation uses
// pread() per read() call (portable fallback); a platform-specific implementation
// may instead prefer sendfile() directly from fd to socket for the common
// "stream to EOF" case, with this interface as the fallback path.
//
// The caller (the streaming HTTP handler) has already resolved TrackId -> absolute
// path through a TrackRepository; this class never accepts a client-supplied path.
class FileStreamSource {
public:
    virtual ~FileStreamSource() = default;

    [[nodiscard]] virtual std::uint64_t fileSize() const = 0;

    // Returns a ResponseBody that yields exactly `length` bytes starting at
    // `offset`. Throws std::runtime_error if the range is out of bounds relative
    // to fileSize() — callers must validate via Range parsing first.
    [[nodiscard]] virtual std::unique_ptr<musicbox::http::ResponseBody>
    openRange(std::uint64_t offset, std::uint64_t length) = 0;
};

// Opens `absolutePath` (already resolved server-side from a TrackId) for
// streaming. Throws std::runtime_error if the file cannot be opened.
[[nodiscard]] std::unique_ptr<FileStreamSource>
openFileStreamSource(const std::string& absolutePath);

} // namespace musicbox::streaming

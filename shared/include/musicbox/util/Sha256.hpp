#pragma once

#include <string>

namespace musicbox::util {

// Hex-encoded SHA-256 digest of a file's contents, streamed in bounded-size
// chunks (never loads the whole file into memory). Used by both the library
// scanner (docs/database.md) and the sync client (Plan.md §12) to detect content
// changes cheaply after a size/mtime pre-check indicates a possible change.
// Throws std::runtime_error if the file cannot be opened.
[[nodiscard]] std::string sha256File(const std::string& absolutePath);

// Hex-encoded SHA-256 digest of an in-memory byte range (used in tests).
[[nodiscard]] std::string sha256Bytes(const void* data, std::size_t size);

} // namespace musicbox::util

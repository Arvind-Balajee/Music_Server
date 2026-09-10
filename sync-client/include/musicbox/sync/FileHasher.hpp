#pragma once

#include <string>

namespace musicbox::sync {

// Abstraction over content hashing. Kept as an interface (rather than calling
// musicbox::util::sha256File directly) so tests can inject a call-counting fake
// to verify the size+mtime pre-check in ManifestDiffer actually skips hashing
// for files it can already prove are unchanged (Plan.md §12, docs/database.md's
// incremental-scan philosophy).
class FileHasher {
public:
    virtual ~FileHasher() = default;

    // Hex-encoded content hash of the file at absolutePath. Implementations may
    // throw std::runtime_error if the file cannot be read.
    [[nodiscard]] virtual std::string hashFile(const std::string& absolutePath) const = 0;
};

// Production hasher: delegates to musicbox::util::sha256File (shared/, streamed,
// bounded memory).
class Sha256FileHasher final : public FileHasher {
public:
    [[nodiscard]] std::string hashFile(const std::string& absolutePath) const override;
};

} // namespace musicbox::sync

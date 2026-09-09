#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "musicbox/db/LibraryRootRepository.hpp"
#include "musicbox/db/TrackRepository.hpp"
#include "musicbox/library/MetadataExtractor.hpp"

namespace musicbox::library {

struct ScanResult {
    std::uint64_t inserted = 0;
    std::uint64_t updated = 0;
    std::uint64_t unchanged = 0;
    std::uint64_t deleted = 0;
    std::uint64_t skippedUnreadable = 0;
};

// Walks a LibraryRoot's absolute path recursively for supported extensions
// (.mp3 .flac .m4a .aac .wav), applying the incremental-rescan rules documented in
// docs/database.md (size+mtime pre-check, content hash only on suspected change,
// soft-delete for files no longer present). Symlinks are not followed.
class LibraryScanner {
public:
    virtual ~LibraryScanner() = default;

    [[nodiscard]] virtual ScanResult scan(const musicbox::LibraryRoot& root) = 0;
};

[[nodiscard]] std::unique_ptr<LibraryScanner> makeLibraryScanner(
    musicbox::db::TrackRepository& trackRepository,
    MetadataExtractor& metadataExtractor);

} // namespace musicbox::library

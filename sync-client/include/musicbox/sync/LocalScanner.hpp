#pragma once

#include <string>
#include <vector>

#include "musicbox/sync/Types.hpp"

namespace musicbox::sync {

// Walks a local directory tree looking for supported audio files (Plan.md §12).
// Mirrors server/include/musicbox/library/LibraryScanner's traversal rules
// (docs/database.md): symlinks are never followed, so this can't be tricked into
// looping or escaping the scanned root.
class LocalScanner {
public:
    // .mp3 .flac .m4a .aac .wav (Plan.md §12), lowercase, leading dot.
    [[nodiscard]] static const std::vector<std::string>& supportedExtensions();

    // Recursively scans rootDir for supported audio files. rootDir must exist and
    // be a directory; throws std::runtime_error otherwise. Returned entries are
    // unordered; relativePath always uses '/' separators regardless of platform.
    [[nodiscard]] std::vector<LocalFileEntry> scan(const std::string& rootDir) const;
};

} // namespace musicbox::sync

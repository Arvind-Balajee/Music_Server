#pragma once

#include <string>
#include <vector>

#include "musicbox/sync/FileHasher.hpp"
#include "musicbox/sync/ServerManifestSource.hpp"
#include "musicbox/sync/Types.hpp"

namespace musicbox::sync {

// The core sync-client deliverable (Plan.md §12): classifies each local file as
// new / modified / unchanged relative to a server manifest, and detects
// server-side files missing locally as deleted.
//
// Mirrors the incremental-scan philosophy documented in docs/database.md's
// LibraryScanner section:
//
//   not on server                                -> new
//   on server, size+mtime match                  -> unchanged (no hash)
//   on server, size or mtime differs              -> hash; hash matches server's
//                                                     contentHash -> unchanged,
//                                                     else -> modified
//   on server, no matching local file             -> deleted
//
// size+mtime is the cheap pre-check; sha256 is only computed when that
// pre-check can't already prove the file is unchanged, or when forceHash is
// requested (`musicbox-sync verify` re-hashes everything to catch silent
// corruption that a matching size+mtime would otherwise hide).
class ManifestDiffer {
public:
    [[nodiscard]] DiffResult diff(const std::vector<LocalFileEntry>& local,
                                   const std::vector<ServerManifestEntry>& server,
                                   const std::string& localRootDir,
                                   FileHasher& hasher,
                                   bool forceHash = false) const;
};

} // namespace musicbox::sync

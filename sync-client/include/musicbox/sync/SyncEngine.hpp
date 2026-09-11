#pragma once

#include <cstddef>
#include <string>

#include "musicbox/sync/FileHasher.hpp"
#include "musicbox/sync/ManifestDiffer.hpp"
#include "musicbox/sync/ServerManifestSource.hpp"
#include "musicbox/sync/Types.hpp"
#include "musicbox/sync/UploadTransport.hpp"

namespace musicbox::sync {

struct PushSummary {
    std::size_t newCount = 0;
    std::size_t modifiedCount = 0;
    std::size_t deletedCount = 0;
    std::size_t unchangedCount = 0;
    std::size_t uploadedOk = 0;
    std::size_t uploadFailed = 0;
};

// Wires LocalScanner + ManifestDiffer + a ServerManifestSource + an
// UploadTransport together for the CLI commands (`push`, `status`, `verify`,
// `list`). Collaborators are taken by reference so callers/tests can supply
// Mock/Null/Fake implementations (Plan.md §25) instead of the Http* ones.
class SyncEngine {
public:
    SyncEngine(ServerManifestSource& serverManifestSource, UploadTransport& uploadTransport,
               FileHasher& hasher);

    // Scans localRootDir, diffs against the server manifest, and uploads every
    // new/modified file (skipped when dryRun is true). Deleted/unchanged files
    // are never uploaded.
    PushSummary push(const std::string& localRootDir, bool dryRun = false);

    // Scans + diffs without uploading anything; used by `list`/`status`/`verify`.
    [[nodiscard]] DiffResult computeDiff(const std::string& localRootDir, bool forceHash = false);

private:
    ServerManifestSource& serverManifestSource_;
    UploadTransport& uploadTransport_;
    FileHasher& hasher_;
};

} // namespace musicbox::sync

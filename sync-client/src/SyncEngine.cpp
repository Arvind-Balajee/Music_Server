#include "musicbox/sync/SyncEngine.hpp"

#include <filesystem>

#include "musicbox/sync/LocalScanner.hpp"

namespace musicbox::sync {

SyncEngine::SyncEngine(ServerManifestSource& serverManifestSource, UploadTransport& uploadTransport,
                       FileHasher& hasher)
    : serverManifestSource_(serverManifestSource), uploadTransport_(uploadTransport),
      hasher_(hasher) {}

DiffResult SyncEngine::computeDiff(const std::string& localRootDir, bool forceHash) {
    const LocalScanner scanner;
    const std::vector<LocalFileEntry> local = scanner.scan(localRootDir);
    const std::vector<ServerManifestEntry> serverManifest = serverManifestSource_.fetchManifest();

    const ManifestDiffer differ;
    return differ.diff(local, serverManifest, localRootDir, hasher_, forceHash);
}

PushSummary SyncEngine::push(const std::string& localRootDir, bool dryRun) {
    const DiffResult diffResult = computeDiff(localRootDir, /*forceHash=*/false);

    PushSummary summary;
    summary.newCount = diffResult.newFiles.size();
    summary.modifiedCount = diffResult.modifiedFiles.size();
    summary.deletedCount = diffResult.deletedFiles.size();
    summary.unchangedCount = diffResult.unchangedFiles.size();

    if (dryRun) {
        return summary;
    }

    const std::filesystem::path root(localRootDir);

    auto uploadAll = [&](const std::vector<std::string>& relativePaths) {
        for (const auto& relativePath : relativePaths) {
            const std::string absolutePath = (root / relativePath).string();
            if (uploadTransport_.uploadFile(relativePath, absolutePath)) {
                ++summary.uploadedOk;
            } else {
                ++summary.uploadFailed;
            }
        }
    };

    uploadAll(diffResult.newFiles);
    uploadAll(diffResult.modifiedFiles);

    return summary;
}

} // namespace musicbox::sync

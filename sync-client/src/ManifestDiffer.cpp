#include "musicbox/sync/ManifestDiffer.hpp"

#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace musicbox::sync {

namespace {

// mtime comparisons are truncated to whole seconds: the proposed wire format
// (docs/api.md) carries the server's mtime as unix epoch seconds, so sub-second
// local precision can never match it exactly and would otherwise force a hash
// on every single file.
std::chrono::system_clock::time_point truncateToSeconds(std::chrono::system_clock::time_point tp) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        std::chrono::time_point_cast<std::chrono::seconds>(tp));
}

std::string joinPath(const std::string& root, const std::string& relativePath) {
    return (std::filesystem::path(root) / relativePath).string();
}

} // namespace

DiffResult ManifestDiffer::diff(const std::vector<LocalFileEntry>& local,
                                const std::vector<ServerManifestEntry>& server,
                                const std::string& localRootDir, FileHasher& hasher,
                                bool forceHash) const {
    DiffResult result;

    std::unordered_map<std::string, const ServerManifestEntry*> serverByPath;
    serverByPath.reserve(server.size());
    for (const auto& entry : server) {
        serverByPath[entry.relativePath] = &entry;
    }

    std::unordered_set<std::string> localPaths;
    localPaths.reserve(local.size());

    for (const auto& localEntry : local) {
        localPaths.insert(localEntry.relativePath);

        const auto it = serverByPath.find(localEntry.relativePath);
        if (it == serverByPath.end()) {
            // Not present on the server at all -> new. (We don't need to hash
            // to know this is new, but a caller uploading it will still hash
            // the bytes as part of the transfer if it wants a content hash.)
            result.newFiles.push_back(localEntry.relativePath);
            continue;
        }

        const ServerManifestEntry& serverEntry = *it->second;

        const bool metadataMatches =
            !forceHash && localEntry.sizeBytes == serverEntry.sizeBytes &&
            truncateToSeconds(localEntry.mtime) == truncateToSeconds(serverEntry.mtime);

        if (metadataMatches) {
            // Cheap pre-check says "definitely unchanged" -- skip hashing.
            result.unchangedFiles.push_back(localEntry.relativePath);
            continue;
        }

        // size/mtime differs (or forceHash was requested for `verify`) -- this
        // is the only case that pays for a SHA-256 read.
        const std::string absolutePath = joinPath(localRootDir, localEntry.relativePath);
        const std::string hash = hasher.hashFile(absolutePath);

        if (hash == serverEntry.contentHash) {
            // e.g. mtime was touched (re-tagged, copied) but content is
            // byte-identical -- still unchanged from the server's perspective.
            result.unchangedFiles.push_back(localEntry.relativePath);
        } else {
            result.modifiedFiles.push_back(localEntry.relativePath);
        }
    }

    for (const auto& [relativePath, unused] : serverByPath) {
        (void)unused;
        if (!localPaths.contains(relativePath)) {
            result.deletedFiles.push_back(relativePath);
        }
    }

    return result;
}

} // namespace musicbox::sync

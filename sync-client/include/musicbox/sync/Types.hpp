#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace musicbox::sync {

// One entry from the local filesystem scan (Plan.md §12). relativePath uses '/'
// separators and is relative to the scanned root directory.
struct LocalFileEntry {
    std::string relativePath;
    std::uint64_t sizeBytes = 0;
    std::chrono::system_clock::time_point mtime{};
};

// Result of diffing a local manifest against a server manifest. Each vector
// holds relative paths; a given path appears in exactly one of the four.
struct DiffResult {
    std::vector<std::string> newFiles;
    std::vector<std::string> modifiedFiles;
    std::vector<std::string> deletedFiles;
    std::vector<std::string> unchangedFiles;

    [[nodiscard]] std::size_t totalFiles() const noexcept {
        return newFiles.size() + modifiedFiles.size() + deletedFiles.size() + unchangedFiles.size();
    }
};

} // namespace musicbox::sync

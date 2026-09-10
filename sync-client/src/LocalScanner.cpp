#include "musicbox/sync/LocalScanner.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace musicbox::sync {

namespace fs = std::filesystem;

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Converts a std::filesystem file time to a system_clock time_point. Uses the
// standard C++20 file_clock::to_sys conversion (P0355), then narrows to
// system_clock's own duration precision (libc++'s file_clock is
// nanosecond-resolution and doesn't implicitly convert).
std::chrono::system_clock::time_point toSystemTime(fs::file_time_type ftime) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(ftime));
}

} // namespace

const std::vector<std::string>& LocalScanner::supportedExtensions() {
    static const std::vector<std::string> kExtensions = {".mp3", ".flac", ".m4a", ".aac", ".wav"};
    return kExtensions;
}

std::vector<LocalFileEntry> LocalScanner::scan(const std::string& rootDir) const {
    const fs::path root(rootDir);

    std::error_code existsEc;
    if (!fs::exists(root, existsEc) || existsEc) {
        throw std::runtime_error("LocalScanner: root directory does not exist: " + rootDir);
    }
    if (!fs::is_directory(root, existsEc) || existsEc) {
        throw std::runtime_error("LocalScanner: root path is not a directory: " + rootDir);
    }

    std::vector<LocalFileEntry> entries;

    // directory_options::skip_permission_denied: a single unreadable
    // subdirectory shouldn't abort the whole scan. follow_directory_symlink is
    // intentionally NOT set (default), so directory symlinks are not
    // descended into -- mirrors the server LibraryScanner (docs/database.md)
    // to avoid traversal loops/escapes outside the configured root.
    const auto options = fs::directory_options::skip_permission_denied;

    for (auto it = fs::recursive_directory_iterator(root, options, existsEc);
         it != fs::recursive_directory_iterator(); it.increment(existsEc)) {
        if (existsEc) {
            // Skip whatever entry triggered the error (e.g. a race with a
            // concurrent delete) and keep scanning.
            existsEc.clear();
            continue;
        }

        const fs::directory_entry& entry = *it;

        std::error_code symlinkEc;
        const bool isSymlink = entry.is_symlink(symlinkEc);
        if (symlinkEc) {
            continue;
        }
        if (isSymlink) {
            // Never follow symlinks -- neither into directories nor as files.
            if (entry.is_directory(symlinkEc)) {
                it.disable_recursion_pending();
            }
            continue;
        }

        std::error_code fileEc;
        if (!entry.is_regular_file(fileEc) || fileEc) {
            continue;
        }

        const std::string extension = toLower(entry.path().extension().string());
        const auto& exts = supportedExtensions();
        if (std::find(exts.begin(), exts.end(), extension) == exts.end()) {
            continue;
        }

        std::error_code sizeEc;
        const std::uint64_t size = entry.file_size(sizeEc);
        if (sizeEc) {
            continue;
        }

        std::error_code timeEc;
        const fs::file_time_type ftime = entry.last_write_time(timeEc);
        if (timeEc) {
            continue;
        }

        LocalFileEntry localEntry;
        localEntry.relativePath = fs::relative(entry.path(), root).generic_string();
        localEntry.sizeBytes = size;
        localEntry.mtime = toSystemTime(ftime);
        entries.push_back(std::move(localEntry));
    }

    return entries;
}

} // namespace musicbox::sync

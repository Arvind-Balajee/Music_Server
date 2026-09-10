#include "musicbox/library/LibraryScanner.hpp"

#include "musicbox/util/Sha256.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace musicbox::library {

namespace {

namespace fs = std::filesystem;

std::string toLowerExtension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool isSupportedExtension(const std::string& lowerExt) {
    static const std::unordered_set<std::string> kSupported = {".mp3", ".flac", ".m4a", ".aac", ".wav"};
    return kSupported.count(lowerExt) != 0;
}

std::chrono::system_clock::time_point toSystemClock(fs::file_time_type fileTime) {
    // file_time_type's clock isn't system_clock; std::chrono::clock_cast
    // (C++20) would be the portable conversion, but isn't implemented by the
    // libc++ version in this environment. file_clock (C++20) guarantees a
    // to_sys/from_sys conversion pair, which every std::filesystem
    // implementation must provide regardless of clock_cast support.
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fs::file_time_type::clock::to_sys(fileTime));
}

class DefaultLibraryScanner : public LibraryScanner {
public:
    DefaultLibraryScanner(musicbox::db::TrackRepository& trackRepository, MetadataExtractor& metadataExtractor)
        : trackRepository_(trackRepository), metadataExtractor_(metadataExtractor) {}

    ScanResult scan(const musicbox::LibraryRoot& root) override {
        ScanResult result;
        std::unordered_set<std::string> seenRelativePaths;

        const fs::path rootPath(root.absolutePath);
        std::error_code walkError;
        fs::recursive_directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, walkError);
        const fs::recursive_directory_iterator end;

        for (; it != end && !walkError; it.increment(walkError)) {
            const fs::directory_entry& entry = *it;

            // Never follow symlinks -- neither into a symlinked directory (the
            // iterator's default directory_options already doesn't recurse
            // through one) nor by indexing a symlinked file, which could
            // otherwise point outside the configured library root
            // (docs/architecture.md §7 path-traversal prevention).
            std::error_code symlinkError;
            if (entry.is_symlink(symlinkError)) {
                continue;
            }

            std::error_code regularFileError;
            if (!entry.is_regular_file(regularFileError) || regularFileError) {
                continue;
            }

            const std::string lowerExt = toLowerExtension(entry.path());
            if (!isSupportedExtension(lowerExt)) {
                continue;
            }

            const std::string relativePath = fs::relative(entry.path(), rootPath).generic_string();
            seenRelativePaths.insert(relativePath);
            processFile(root, entry, relativePath, result);
        }

        if (walkError) {
            std::cerr << "LibraryScanner: error walking " << root.absolutePath << ": " << walkError.message()
                       << '\n';
        }

        detectDeletions(root, seenRelativePaths, result);
        return result;
    }

private:
    void processFile(const musicbox::LibraryRoot& root, const fs::directory_entry& entry,
                      const std::string& relativePath, ScanResult& result) {
        std::error_code sizeError;
        const std::uint64_t fileSize = entry.file_size(sizeError);
        std::error_code mtimeError;
        const fs::file_time_type mtime = entry.last_write_time(mtimeError);
        if (sizeError || mtimeError) {
            result.skippedUnreadable++;
            return;
        }
        const auto modifiedAt = toSystemClock(mtime);

        std::optional<musicbox::Track> existing = trackRepository_.findByPath(root.id, relativePath);

        if (!existing.has_value()) {
            insertNew(root, entry, relativePath, fileSize, modifiedAt, result);
            return;
        }

        // A file that reappeared after being soft-deleted is treated like a
        // fresh appearance regardless of what size+mtime happen to say --
        // there's no meaningful "unchanged since" baseline for a row that was
        // marked gone in between. Counted as `inserted` from the caller's
        // point of view (a newly-visible track), even though the DB row is
        // reused per the UNIQUE(library_root_id, relative_path) constraint
        // (docs/database.md).
        if (existing->deletedAt.has_value()) {
            insertNew(root, entry, relativePath, fileSize, modifiedAt, result);
            return;
        }

        // Compare at millisecond granularity: `modifiedAt` here comes straight
        // off the filesystem clock (often micro/nanosecond resolution on
        // macOS/Linux), but `existing->modifiedAt` already round-tripped
        // through the database's INTEGER-epoch-milliseconds storage
        // (docs/database.md) and lost anything finer than a millisecond.
        // Comparing the raw time_points directly would make possiblyChanged
        // true on almost every rescan even when nothing changed, defeating
        // the entire point of this cheap pre-check.
        const auto toMillis = [](std::chrono::system_clock::time_point tp) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        };
        const bool possiblyChanged =
            fileSize != existing->fileSizeBytes || toMillis(modifiedAt) != toMillis(existing->modifiedAt);
        if (!possiblyChanged) {
            result.unchanged++;
            return;
        }

        // size/mtime pre-check flagged a possible change -- hash to find out
        // whether the content actually differs before paying for a full
        // metadata re-extraction (docs/database.md's incremental-scan rule).
        std::string hash;
        try {
            hash = musicbox::util::sha256File(entry.path().string());
        } catch (const std::exception& e) {
            std::cerr << "LibraryScanner: failed to hash " << entry.path() << ": " << e.what() << '\n';
            result.skippedUnreadable++;
            return;
        }

        // Either the content genuinely changed, or only mtime moved (e.g. a
        // touch, or a copy that preserved bytes) but the hash still matches.
        // Either way we need a full TrackUpsert to persist the refreshed
        // size/mtime (so a future scan's cheap pre-check doesn't keep
        // re-hashing this file) -- and TrackUpsert::artistName/albumTitle are
        // *names*, not the existing row's resolved artist_id/album_id
        // (TrackRepository::upsert unconditionally overwrites artist_id/
        // album_id from whatever name is passed in, per
        // docs/adr/0006-track-repository-write-methods.md), so re-extracting
        // is the only safe way to get a correct TrackUpsert here -- there's no
        // cheaper "just update these two columns" path available through this
        // interface. Extraction (tag reading) is comparatively cheap next to
        // the hashing we already paid for above.
        auto metadata = metadataExtractor_.extract(entry.path().string());
        if (!metadata.has_value()) {
            result.skippedUnreadable++;
            return;
        }
        auto upsertData = toUpsert(*metadata, root.id, relativePath, fileSize, modifiedAt, hash);
        trackRepository_.upsert(upsertData);

        if (hash == existing->contentHash) {
            result.unchanged++;
        } else {
            result.updated++;
        }
    }

    void insertNew(const musicbox::LibraryRoot& root, const fs::directory_entry& entry,
                    const std::string& relativePath, std::uint64_t fileSize,
                    std::chrono::system_clock::time_point modifiedAt, ScanResult& result) {
        auto metadata = metadataExtractor_.extract(entry.path().string());
        if (!metadata.has_value()) {
            result.skippedUnreadable++;
            return;
        }

        std::string hash;
        try {
            hash = musicbox::util::sha256File(entry.path().string());
        } catch (const std::exception& e) {
            std::cerr << "LibraryScanner: failed to hash " << entry.path() << ": " << e.what() << '\n';
            result.skippedUnreadable++;
            return;
        }

        auto upsertData = toUpsert(*metadata, root.id, relativePath, fileSize, modifiedAt, hash);
        trackRepository_.upsert(upsertData);
        result.inserted++;
    }

    static musicbox::db::TrackUpsert toUpsert(const RawTrackMetadata& metadata, musicbox::LibraryRootId rootId,
                                                const std::string& relativePath, std::uint64_t fileSize,
                                                std::chrono::system_clock::time_point modifiedAt,
                                                const std::string& contentHash) {
        musicbox::db::TrackUpsert data;
        data.libraryRootId = rootId;
        data.relativePath = relativePath;
        data.title = metadata.title;
        data.artistName = metadata.artist;
        data.albumTitle = metadata.album;
        data.albumArtist = metadata.albumArtist;
        data.trackNumber = metadata.trackNumber;
        data.discNumber = metadata.discNumber;
        data.year = metadata.year;
        data.genre = metadata.genre;
        data.duration = std::chrono::milliseconds(metadata.durationMs);
        data.codec = metadata.codec;
        data.bitrateKbps = metadata.bitrateKbps;
        data.fileSizeBytes = fileSize;
        data.contentHash = contentHash;
        data.modifiedAt = modifiedAt;
        data.hasArtwork = metadata.hasArtwork;
        return data;
    }

    void detectDeletions(const musicbox::LibraryRoot& root, const std::unordered_set<std::string>& seenRelativePaths,
                          ScanResult& result) {
        const auto now = std::chrono::system_clock::now();
        for (const musicbox::Track& track : trackRepository_.listByLibraryRoot(root.id)) {
            if (track.deletedAt.has_value()) {
                continue; // already soft-deleted
            }
            if (seenRelativePaths.count(track.relativePath) != 0) {
                continue; // still present on disk
            }
            if (trackRepository_.softDelete(track.id, now)) {
                result.deleted++;
            }
        }
    }

    musicbox::db::TrackRepository& trackRepository_;
    MetadataExtractor& metadataExtractor_;
};

} // namespace

std::unique_ptr<LibraryScanner> makeLibraryScanner(musicbox::db::TrackRepository& trackRepository,
                                                     MetadataExtractor& metadataExtractor) {
    return std::make_unique<DefaultLibraryScanner>(trackRepository, metadataExtractor);
}

} // namespace musicbox::library

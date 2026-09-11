#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "musicbox/model/Ids.hpp"

namespace musicbox {

// A single indexed audio file. `relativePath` is relative to its LibraryRoot and
// is an internal implementation detail: it must never be sent to a client or
// accepted from one. Clients address tracks only by TrackId (see docs/api.md and
// docs/architecture.md §7 on path-traversal prevention).
struct Track {
    TrackId id;
    LibraryRootId libraryRootId;

    std::string title;
    std::optional<ArtistId> artistId;
    std::optional<AlbumId> albumId;

    std::optional<std::string> albumArtist;
    std::optional<int> trackNumber;
    std::optional<int> discNumber;
    std::optional<int> year;
    std::optional<std::string> genre;

    std::chrono::milliseconds duration{0};

    std::string codec; // "mp3", "flac", "alac", "aac", "wav"
    std::optional<int> bitrateKbps;

    std::uint64_t fileSizeBytes = 0;
    std::string relativePath; // relative to libraryRootId's absolute path; never exposed to clients
    std::string contentHash;  // SHA-256 hex digest, used for incremental scan/sync
    std::chrono::system_clock::time_point modifiedAt;

    bool hasArtwork = false;

    // Soft-delete marker: set when the file is no longer found on disk during a
    // scan. Absent (nullopt) for tracks currently present.
    std::optional<std::chrono::system_clock::time_point> deletedAt;
};

} // namespace musicbox

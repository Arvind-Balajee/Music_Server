#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Ids.hpp"
#include "musicbox/model/Track.hpp"

namespace musicbox::db {

struct TrackQuery {
    std::optional<musicbox::ArtistId> artistId;
    std::optional<musicbox::AlbumId> albumId;
    std::optional<std::string> search; // case-insensitive match over title/artist/album
    std::size_t limit = 100;
    std::size_t offset = 0;
};

// Everything the LibraryScanner (docs/database.md "Library Scanner") knows about
// a track after extracting metadata from a file, prior to the artist/album rows
// existing. `artistName`/`albumTitle` are resolved (find-or-create) by the
// repository implementation itself, so the scanner never needs direct access to
// ArtistRepository/AlbumRepository (it is only handed a TrackRepository and a
// MetadataExtractor — see LibraryScanner.hpp) and two repository instances never
// end up racing to insert the same artist/album row from different connections.
struct TrackUpsert {
    musicbox::LibraryRootId libraryRootId;
    std::string relativePath; // relative to the library root; identifies the row

    std::string title;
    std::optional<std::string> artistName;
    std::optional<std::string> albumTitle;
    std::optional<std::string>
        albumArtist; // resolved the same way as artistName for the album's album_artist_id
    std::optional<int> trackNumber;
    std::optional<int> discNumber;
    std::optional<int> year;
    std::optional<std::string> genre;

    std::chrono::milliseconds duration{0};
    std::string codec;
    std::optional<int> bitrateKbps;

    std::uint64_t fileSizeBytes = 0;
    std::string contentHash;
    std::chrono::system_clock::time_point modifiedAt;
    bool hasArtwork = false;
};

// The API layer (docs/api.md) depends only on this interface, never on SQLite
// directly. SqliteTrackRepository (Agent 3) is the production implementation;
// tests use an in-memory fake. One instance per worker thread — see
// docs/database.md and docs/architecture.md §4.
//
// docs/adr/0006-track-repository-write-methods.md: upsert/findByPath/
// listByLibraryRoot/softDelete were added alongside the original read-only
// sketch in docs/database.md so LibraryScanner has a write path without a
// dependency on ArtistRepository/AlbumRepository (see TrackUpsert above).
class TrackRepository {
public:
    virtual ~TrackRepository() = default;

    [[nodiscard]] virtual std::optional<musicbox::Track> findById(musicbox::TrackId id) = 0;
    [[nodiscard]] virtual std::vector<musicbox::Track> list(const TrackQuery& query) = 0;
    [[nodiscard]] virtual std::size_t count(const TrackQuery& query) = 0;

    // Resolves a track to an absolute filesystem path for streaming. This is the
    // only sanctioned way a path reaches the streaming handler — never accept a
    // path from client input (docs/architecture.md §7).
    [[nodiscard]] virtual std::optional<std::string> resolveAbsolutePath(musicbox::TrackId id) = 0;

    // Finds the track for a given (libraryRootId, relativePath), including
    // soft-deleted rows (a file that reappears after being deleted must reuse
    // the same row per the tracks.UNIQUE(library_root_id, relative_path)
    // constraint — see docs/database.md).
    [[nodiscard]] virtual std::optional<musicbox::Track>
    findByPath(musicbox::LibraryRootId libraryRootId, const std::string& relativePath) = 0;

    // All tracks under a root, including soft-deleted ones. The LibraryScanner
    // uses this to detect files that were indexed but are no longer present on
    // disk.
    [[nodiscard]] virtual std::vector<musicbox::Track>
    listByLibraryRoot(musicbox::LibraryRootId libraryRootId) = 0;

    // Inserts a new track row, or updates the existing one for
    // (libraryRootId, relativePath) and clears deleted_at if it was set.
    // Find-or-creates the artist/album rows implied by artistName/albumTitle.
    virtual musicbox::Track upsert(const TrackUpsert& data) = 0;

    // Soft-delete: sets deleted_at = when. Returns false if the track doesn't
    // exist or was already deleted.
    virtual bool softDelete(musicbox::TrackId id, std::chrono::system_clock::time_point when) = 0;
};

// Constructs the production SQLite-backed implementation. `databasePath` is
// opened directly by SQLite -- a plain filesystem path, ":memory:", or a URI
// (e.g. "file:name?mode=memory&cache=shared" for tests that need multiple
// repository instances to see the same in-memory database). Applies pending
// migrations before returning. One instance per worker thread; never share
// across threads (see class comment above).
[[nodiscard]] std::unique_ptr<TrackRepository>
makeSqliteTrackRepository(const std::string& databasePath);

} // namespace musicbox::db

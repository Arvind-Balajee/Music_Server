#pragma once

#include <cstddef>
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

// The API layer (docs/api.md) depends only on this interface, never on SQLite
// directly. SqliteTrackRepository (Agent 3) is the production implementation;
// tests use an in-memory fake. One instance per worker thread — see
// docs/database.md and docs/architecture.md §4.
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
};

} // namespace musicbox::db

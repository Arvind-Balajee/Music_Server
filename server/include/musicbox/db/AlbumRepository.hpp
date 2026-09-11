#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "musicbox/model/Album.hpp"
#include "musicbox/model/Ids.hpp"

namespace musicbox::db {

struct AlbumQuery {
    std::optional<musicbox::ArtistId> artistId;
    std::optional<std::string> search;
    std::size_t limit = 100;
    std::size_t offset = 0;
};

class AlbumRepository {
public:
    virtual ~AlbumRepository() = default;

    [[nodiscard]] virtual std::optional<musicbox::Album> findById(musicbox::AlbumId id) = 0;
    [[nodiscard]] virtual std::vector<musicbox::Album> list(const AlbumQuery& query) = 0;
    [[nodiscard]] virtual std::size_t count(const AlbumQuery& query) = 0;
};

// See TrackRepository.hpp's makeSqliteTrackRepository for the databasePath
// contract. Read-only (docs/adr/0006-track-repository-write-methods.md):
// album rows are created only via TrackRepository::upsert.
[[nodiscard]] std::unique_ptr<AlbumRepository>
makeSqliteAlbumRepository(const std::string& databasePath);

} // namespace musicbox::db

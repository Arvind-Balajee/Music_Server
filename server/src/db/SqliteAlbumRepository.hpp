#pragma once

#include <string>

#include "musicbox/db/AlbumRepository.hpp"

#include "SqliteConnection.hpp"

namespace musicbox::db {

// Read-only (see docs/adr/0006-track-repository-write-methods.md): album rows
// are created only via TrackRepository::upsert's find-or-create step during a
// library scan.
class SqliteAlbumRepository final : public AlbumRepository {
public:
    explicit SqliteAlbumRepository(const std::string& databasePath);

    [[nodiscard]] std::optional<musicbox::Album> findById(musicbox::AlbumId id) override;
    [[nodiscard]] std::vector<musicbox::Album> list(const AlbumQuery& query) override;
    [[nodiscard]] std::size_t count(const AlbumQuery& query) override;

private:
    SqliteConnection connection_;
};

} // namespace musicbox::db

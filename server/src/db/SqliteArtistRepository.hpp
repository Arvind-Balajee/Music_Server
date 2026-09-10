#pragma once

#include <string>

#include "musicbox/db/ArtistRepository.hpp"

#include "SqliteConnection.hpp"

namespace musicbox::db {

// Read-only (see docs/adr/0006-track-repository-write-methods.md): artist rows
// are created only via TrackRepository::upsert's find-or-create step during a
// library scan.
class SqliteArtistRepository final : public ArtistRepository {
public:
    explicit SqliteArtistRepository(const std::string& databasePath);

    [[nodiscard]] std::optional<musicbox::Artist> findById(musicbox::ArtistId id) override;
    [[nodiscard]] std::vector<musicbox::Artist> list(const ArtistQuery& query) override;
    [[nodiscard]] std::size_t count(const ArtistQuery& query) override;

private:
    SqliteConnection connection_;
};

} // namespace musicbox::db

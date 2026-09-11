#pragma once

#include <string>

#include "musicbox/db/TrackRepository.hpp"

#include "SqliteConnection.hpp"

namespace musicbox::db {

// One sqlite3* connection per instance (docs/database.md, docs/architecture.md
// §4) -- construct one per worker thread, never share an instance across
// threads. `databasePath` is passed straight to SqliteConnection, so a
// shared-cache URI (see SqliteConnection.hpp) works for tests that need
// multiple repository instances to see the same in-memory database.
class SqliteTrackRepository final : public TrackRepository {
public:
    explicit SqliteTrackRepository(const std::string& databasePath);

    [[nodiscard]] std::optional<musicbox::Track> findById(musicbox::TrackId id) override;
    [[nodiscard]] std::vector<musicbox::Track> list(const TrackQuery& query) override;
    [[nodiscard]] std::size_t count(const TrackQuery& query) override;
    [[nodiscard]] std::optional<std::string> resolveAbsolutePath(musicbox::TrackId id) override;
    [[nodiscard]] std::optional<musicbox::Track>
    findByPath(musicbox::LibraryRootId libraryRootId, const std::string& relativePath) override;
    [[nodiscard]] std::vector<musicbox::Track>
    listByLibraryRoot(musicbox::LibraryRootId libraryRootId) override;
    musicbox::Track upsert(const TrackUpsert& data) override;
    bool softDelete(musicbox::TrackId id, std::chrono::system_clock::time_point when) override;

private:
    SqliteConnection connection_;
};

} // namespace musicbox::db

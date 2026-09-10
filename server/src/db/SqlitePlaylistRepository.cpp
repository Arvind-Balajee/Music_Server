#include "SqlitePlaylistRepository.hpp"

#include "MigrationRunner.hpp"
#include "SqliteStatement.hpp"
#include "TimeUtil.hpp"
#include "TrackRow.hpp"

namespace musicbox::db {

namespace {

musicbox::Playlist mapPlaylistRow(SqliteStatement& stmt) {
    musicbox::Playlist playlist;
    playlist.id = musicbox::PlaylistId(stmt.columnInt64(0));
    playlist.name = stmt.columnText(1);
    playlist.createdAt = fromEpochMillis(stmt.columnInt64(2));
    playlist.updatedAt = fromEpochMillis(stmt.columnInt64(3));
    return playlist;
}

bool rowExists(SqliteConnection& connection, const std::string& table, std::int64_t id) {
    SqliteStatement stmt(connection, "SELECT 1 FROM " + table + " WHERE id = ?;");
    stmt.bindInt64(1, id);
    return stmt.step();
}

} // namespace

SqlitePlaylistRepository::SqlitePlaylistRepository(const std::string& databasePath) : connection_(databasePath) {
    migrate(connection_);
}

std::optional<musicbox::Playlist> SqlitePlaylistRepository::findById(musicbox::PlaylistId id) {
    SqliteStatement stmt(connection_, "SELECT id, name, created_at, updated_at FROM playlists WHERE id = ?;");
    stmt.bindInt64(1, id.value());
    if (!stmt.step()) {
        return std::nullopt;
    }
    return mapPlaylistRow(stmt);
}

std::vector<musicbox::Playlist> SqlitePlaylistRepository::list(std::size_t limit, std::size_t offset) {
    SqliteStatement stmt(connection_,
                          "SELECT id, name, created_at, updated_at FROM playlists ORDER BY id LIMIT ? OFFSET ?;");
    stmt.bindInt64(1, static_cast<std::int64_t>(limit));
    stmt.bindInt64(2, static_cast<std::int64_t>(offset));

    std::vector<musicbox::Playlist> results;
    while (stmt.step()) {
        results.push_back(mapPlaylistRow(stmt));
    }
    return results;
}

musicbox::Playlist SqlitePlaylistRepository::create(std::string name) {
    const auto now = std::chrono::system_clock::now();
    SqliteStatement insert(connection_, "INSERT INTO playlists(name, created_at, updated_at) VALUES(?, ?, ?);");
    insert.bindText(1, name);
    insert.bindInt64(2, toEpochMillis(now));
    insert.bindInt64(3, toEpochMillis(now));
    insert.run();

    musicbox::Playlist playlist;
    playlist.id = musicbox::PlaylistId(connection_.lastInsertRowId());
    playlist.name = std::move(name);
    playlist.createdAt = now;
    playlist.updatedAt = now;
    return playlist;
}

bool SqlitePlaylistRepository::rename(musicbox::PlaylistId id, std::string newName) {
    SqliteStatement stmt(connection_, "UPDATE playlists SET name = ?, updated_at = ? WHERE id = ?;");
    stmt.bindText(1, newName);
    stmt.bindInt64(2, toEpochMillis(std::chrono::system_clock::now()));
    stmt.bindInt64(3, id.value());
    stmt.run();
    return connection_.changes() > 0;
}

bool SqlitePlaylistRepository::remove(musicbox::PlaylistId id) {
    // playlist_tracks rows for this playlist are removed automatically via
    // ON DELETE CASCADE (foreign_keys=ON is set for every connection in
    // SqliteConnection's constructor).
    SqliteStatement stmt(connection_, "DELETE FROM playlists WHERE id = ?;");
    stmt.bindInt64(1, id.value());
    stmt.run();
    return connection_.changes() > 0;
}

std::vector<musicbox::Track> SqlitePlaylistRepository::tracks(musicbox::PlaylistId id) {
    SqliteStatement stmt(connection_, std::string("SELECT ") + kTrackColumns +
                                            " FROM tracks JOIN playlist_tracks "
                                            "ON tracks.id = playlist_tracks.track_id "
                                            "WHERE playlist_tracks.playlist_id = ? "
                                            "ORDER BY playlist_tracks.position;");
    stmt.bindInt64(1, id.value());

    std::vector<musicbox::Track> results;
    while (stmt.step()) {
        results.push_back(mapTrackRow(stmt));
    }
    return results;
}

bool SqlitePlaylistRepository::addTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) {
    SqliteTransaction transaction(connection_);

    if (!rowExists(connection_, "playlists", id.value()) || !rowExists(connection_, "tracks", trackId.value())) {
        return false;
    }

    // Already a member -- treat as a no-op success rather than a duplicate
    // (playlist_tracks' primary key is (playlist_id, track_id), so a naive
    // INSERT would otherwise throw a constraint-violation exception here).
    SqliteStatement existing(connection_,
                              "SELECT 1 FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?;");
    existing.bindInt64(1, id.value());
    existing.bindInt64(2, trackId.value());
    if (existing.step()) {
        // Must reset before COMMIT -- SQLite refuses to commit while any
        // statement on the connection is still mid-result.
        existing.reset();
        transaction.commit();
        return true;
    }

    SqliteStatement nextPosition(connection_,
                                  "SELECT COALESCE(MAX(position) + 1, 0) FROM playlist_tracks WHERE playlist_id = ?;");
    nextPosition.bindInt64(1, id.value());
    nextPosition.step();
    const std::int64_t position = nextPosition.columnInt64(0);
    nextPosition.reset(); // same reason -- stays alive while insert/touch run below

    SqliteStatement insert(connection_,
                            "INSERT INTO playlist_tracks(playlist_id, track_id, position) VALUES(?, ?, ?);");
    insert.bindInt64(1, id.value());
    insert.bindInt64(2, trackId.value());
    insert.bindInt64(3, position);
    insert.run();

    SqliteStatement touch(connection_, "UPDATE playlists SET updated_at = ? WHERE id = ?;");
    touch.bindInt64(1, toEpochMillis(std::chrono::system_clock::now()));
    touch.bindInt64(2, id.value());
    touch.run();

    transaction.commit();
    return true;
}

bool SqlitePlaylistRepository::removeTrack(musicbox::PlaylistId id, musicbox::TrackId trackId) {
    SqliteTransaction transaction(connection_);

    SqliteStatement stmt(connection_, "DELETE FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?;");
    stmt.bindInt64(1, id.value());
    stmt.bindInt64(2, trackId.value());
    stmt.run();
    const bool removed = connection_.changes() > 0;

    if (removed) {
        // position is a dense-but-gaps-tolerated ordering for the MVP
        // (docs/database.md): removing a track leaves a gap rather than
        // renumbering the remainder, since ORDER BY position already produces
        // the correct sequence regardless of gaps.
        SqliteStatement touch(connection_, "UPDATE playlists SET updated_at = ? WHERE id = ?;");
        touch.bindInt64(1, toEpochMillis(std::chrono::system_clock::now()));
        touch.bindInt64(2, id.value());
        touch.run();
    }

    transaction.commit();
    return removed;
}

std::unique_ptr<PlaylistRepository> makeSqlitePlaylistRepository(const std::string& databasePath) {
    return std::make_unique<SqlitePlaylistRepository>(databasePath);
}

} // namespace musicbox::db

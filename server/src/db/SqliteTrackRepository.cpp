#include "SqliteTrackRepository.hpp"

#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "MigrationRunner.hpp"
#include "SqliteStatement.hpp"
#include "TimeUtil.hpp"
#include "TrackRow.hpp"

namespace musicbox::db {

namespace {

std::optional<std::int64_t> toOptionalInt64(std::optional<int> value) {
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(*value);
}

// Find-or-create by unique name. Must run inside a transaction the caller
// already holds (see SqliteTrackRepository::upsert) so the SELECT-then-INSERT
// isn't racy against another writer.
std::optional<musicbox::ArtistId> resolveArtist(SqliteConnection& connection,
                                                const std::optional<std::string>& name) {
    if (!name.has_value() || name->empty()) {
        return std::nullopt;
    }

    SqliteStatement select(connection, "SELECT id FROM artists WHERE name = ?;");
    select.bindText(1, *name);
    if (select.step()) {
        return musicbox::ArtistId(select.columnInt64(0));
    }

    SqliteStatement insert(connection, "INSERT INTO artists(name) VALUES(?);");
    insert.bindText(1, *name);
    insert.run();
    return musicbox::ArtistId(connection.lastInsertRowId());
}

// Find-or-create by (title, albumArtistId), treating a missing albumArtistId
// as its own distinct identity (IS NULL) rather than relying on the schema's
// UNIQUE(title, album_artist_id) index, which -- per SQL semantics -- would
// treat every NULL album_artist_id as distinct and never collapse duplicates
// (see docs/adr/0006-track-repository-write-methods.md).
std::optional<musicbox::AlbumId> resolveAlbum(SqliteConnection& connection,
                                              const std::optional<std::string>& title,
                                              std::optional<musicbox::ArtistId> albumArtistId) {
    if (!title.has_value() || title->empty()) {
        return std::nullopt;
    }

    const char* selectSql =
        albumArtistId.has_value()
            ? "SELECT id FROM albums WHERE title = ? AND album_artist_id = ?;"
            : "SELECT id FROM albums WHERE title = ? AND album_artist_id IS NULL;";
    SqliteStatement select(connection, selectSql);
    select.bindText(1, *title);
    if (albumArtistId.has_value()) {
        select.bindInt64(2, albumArtistId->value());
    }
    if (select.step()) {
        return musicbox::AlbumId(select.columnInt64(0));
    }

    SqliteStatement insert(connection, "INSERT INTO albums(title, album_artist_id) VALUES(?, ?);");
    insert.bindText(1, *title);
    insert.bindOptionalInt64(2, albumArtistId.has_value()
                                    ? std::optional<std::int64_t>(albumArtistId->value())
                                    : std::nullopt);
    insert.run();
    return musicbox::AlbumId(connection.lastInsertRowId());
}

} // namespace

SqliteTrackRepository::SqliteTrackRepository(const std::string& databasePath)
    : connection_(databasePath) {
    migrate(connection_);
}

std::optional<musicbox::Track> SqliteTrackRepository::findById(musicbox::TrackId id) {
    SqliteStatement stmt(connection_,
                         std::string("SELECT ") + kTrackColumns + " FROM tracks WHERE id = ?;");
    stmt.bindInt64(1, id.value());
    if (!stmt.step()) {
        return std::nullopt;
    }
    return mapTrackRow(stmt);
}

namespace {

// Both list() and count() filter identically; ?1=artistId, ?2=albumId,
// ?3=search pattern (already wrapped in '%...%') or NULL to disable a filter.
void bindTrackQueryFilters(SqliteStatement& stmt, const TrackQuery& query) {
    stmt.bindOptionalInt64(1, query.artistId.has_value()
                                  ? std::optional<std::int64_t>(query.artistId->value())
                                  : std::nullopt);
    stmt.bindOptionalInt64(2, query.albumId.has_value()
                                  ? std::optional<std::int64_t>(query.albumId->value())
                                  : std::nullopt);
    if (query.search.has_value()) {
        stmt.bindText(3, "%" + *query.search + "%");
    } else {
        stmt.bindNull(3);
    }
}

constexpr const char* kTrackFilterWhere =
    "WHERE t.deleted_at IS NULL "
    "AND (?1 IS NULL OR t.artist_id = ?1) "
    "AND (?2 IS NULL OR t.album_id = ?2) "
    "AND (?3 IS NULL OR t.title LIKE ?3 COLLATE NOCASE OR ar.name LIKE ?3 COLLATE NOCASE OR "
    "al.title LIKE ?3 COLLATE NOCASE)";

} // namespace

std::vector<musicbox::Track> SqliteTrackRepository::list(const TrackQuery& query) {
    std::ostringstream sql;
    sql << "SELECT t.id, t.library_root_id, t.title, t.artist_id, t.album_id, t.track_number, "
           "t.disc_number, "
           "t.year, t.genre, t.duration_ms, t.codec, t.bitrate_kbps, t.file_size_bytes, "
           "t.relative_path, "
           "t.content_hash, t.modified_at, t.has_artwork, t.deleted_at "
           "FROM tracks t "
           "LEFT JOIN artists ar ON ar.id = t.artist_id "
           "LEFT JOIN albums al ON al.id = t.album_id "
        << kTrackFilterWhere << " ORDER BY t.id LIMIT ?4 OFFSET ?5;";

    SqliteStatement stmt(connection_, sql.str());
    bindTrackQueryFilters(stmt, query);
    stmt.bindInt64(4, static_cast<std::int64_t>(query.limit));
    stmt.bindInt64(5, static_cast<std::int64_t>(query.offset));

    std::vector<musicbox::Track> results;
    while (stmt.step()) {
        results.push_back(mapTrackRow(stmt));
    }
    return results;
}

std::size_t SqliteTrackRepository::count(const TrackQuery& query) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM tracks t "
           "LEFT JOIN artists ar ON ar.id = t.artist_id "
           "LEFT JOIN albums al ON al.id = t.album_id "
        << kTrackFilterWhere << ";";

    SqliteStatement stmt(connection_, sql.str());
    bindTrackQueryFilters(stmt, query);

    stmt.step();
    return static_cast<std::size_t>(stmt.columnInt64(0));
}

std::optional<std::string> SqliteTrackRepository::resolveAbsolutePath(musicbox::TrackId id) {
    SqliteStatement stmt(connection_, "SELECT lr.path, t.relative_path FROM tracks t "
                                      "JOIN library_roots lr ON lr.id = t.library_root_id "
                                      "WHERE t.id = ? AND t.deleted_at IS NULL;");
    stmt.bindInt64(1, id.value());
    if (!stmt.step()) {
        return std::nullopt;
    }
    const std::filesystem::path root = stmt.columnText(0);
    const std::filesystem::path relative = stmt.columnText(1);
    return (root / relative).string();
}

std::optional<musicbox::Track>
SqliteTrackRepository::findByPath(musicbox::LibraryRootId libraryRootId,
                                  const std::string& relativePath) {
    SqliteStatement stmt(connection_,
                         std::string("SELECT ") + kTrackColumns +
                             " FROM tracks WHERE library_root_id = ? AND relative_path = ?;");
    stmt.bindInt64(1, libraryRootId.value());
    stmt.bindText(2, relativePath);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return mapTrackRow(stmt);
}

std::vector<musicbox::Track>
SqliteTrackRepository::listByLibraryRoot(musicbox::LibraryRootId libraryRootId) {
    SqliteStatement stmt(connection_, std::string("SELECT ") + kTrackColumns +
                                          " FROM tracks WHERE library_root_id = ?;");
    stmt.bindInt64(1, libraryRootId.value());

    std::vector<musicbox::Track> results;
    while (stmt.step()) {
        results.push_back(mapTrackRow(stmt));
    }
    return results;
}

musicbox::Track SqliteTrackRepository::upsert(const TrackUpsert& data) {
    SqliteTransaction transaction(connection_);

    const auto artistId = resolveArtist(connection_, data.artistName);
    // A track without an explicit album artist tag conventionally shares the
    // track artist as its album's artist (the common single-artist-album
    // case); an explicit albumArtist tag always wins.
    const auto albumArtistId =
        data.albumArtist.has_value() ? resolveArtist(connection_, data.albumArtist) : artistId;
    const auto albumId = resolveAlbum(connection_, data.albumTitle, albumArtistId);

    SqliteStatement upsertStmt(connection_, R"SQL(
        INSERT INTO tracks (
            library_root_id, title, artist_id, album_id, track_number, disc_number,
            year, genre, duration_ms, codec, bitrate_kbps, file_size_bytes,
            relative_path, content_hash, modified_at, has_artwork, deleted_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NULL)
        ON CONFLICT(library_root_id, relative_path) DO UPDATE SET
            title = excluded.title,
            artist_id = excluded.artist_id,
            album_id = excluded.album_id,
            track_number = excluded.track_number,
            disc_number = excluded.disc_number,
            year = excluded.year,
            genre = excluded.genre,
            duration_ms = excluded.duration_ms,
            codec = excluded.codec,
            bitrate_kbps = excluded.bitrate_kbps,
            file_size_bytes = excluded.file_size_bytes,
            content_hash = excluded.content_hash,
            modified_at = excluded.modified_at,
            has_artwork = excluded.has_artwork,
            deleted_at = NULL
        RETURNING id;
    )SQL");
    upsertStmt.bindInt64(1, data.libraryRootId.value());
    upsertStmt.bindText(2, data.title);
    upsertStmt.bindOptionalInt64(
        3, artistId.has_value() ? std::optional<std::int64_t>(artistId->value()) : std::nullopt);
    upsertStmt.bindOptionalInt64(
        4, albumId.has_value() ? std::optional<std::int64_t>(albumId->value()) : std::nullopt);
    upsertStmt.bindOptionalInt64(5, toOptionalInt64(data.trackNumber));
    upsertStmt.bindOptionalInt64(6, toOptionalInt64(data.discNumber));
    upsertStmt.bindOptionalInt64(7, toOptionalInt64(data.year));
    upsertStmt.bindOptionalText(8, data.genre);
    upsertStmt.bindInt64(9, data.duration.count());
    upsertStmt.bindText(10, data.codec);
    upsertStmt.bindOptionalInt64(11, toOptionalInt64(data.bitrateKbps));
    upsertStmt.bindInt64(12, static_cast<std::int64_t>(data.fileSizeBytes));
    upsertStmt.bindText(13, data.relativePath);
    upsertStmt.bindText(14, data.contentHash);
    upsertStmt.bindInt64(15, toEpochMillis(data.modifiedAt));
    upsertStmt.bindInt64(16, data.hasArtwork ? 1 : 0);

    if (!upsertStmt.step()) {
        throw std::runtime_error(
            "SqliteTrackRepository::upsert: INSERT ... RETURNING produced no row");
    }
    const auto id = musicbox::TrackId(upsertStmt.columnInt64(0));
    // SQLite refuses to COMMIT while any statement on the connection is still
    // mid-result (as RETURNING leaves this one after step() returns a row) --
    // reset() drops it back to "not in progress" without losing the value
    // already read above.
    upsertStmt.reset();

    transaction.commit();

    musicbox::Track track;
    track.id = id;
    track.libraryRootId = data.libraryRootId;
    track.title = data.title;
    track.artistId = artistId;
    track.albumId = albumId;
    track.albumArtist = data.albumArtist;
    track.trackNumber = data.trackNumber;
    track.discNumber = data.discNumber;
    track.year = data.year;
    track.genre = data.genre;
    track.duration = data.duration;
    track.codec = data.codec;
    track.bitrateKbps = data.bitrateKbps;
    track.fileSizeBytes = data.fileSizeBytes;
    track.relativePath = data.relativePath;
    track.contentHash = data.contentHash;
    track.modifiedAt = data.modifiedAt;
    track.hasArtwork = data.hasArtwork;
    track.deletedAt = std::nullopt;
    return track;
}

bool SqliteTrackRepository::softDelete(musicbox::TrackId id,
                                       std::chrono::system_clock::time_point when) {
    SqliteStatement stmt(connection_,
                         "UPDATE tracks SET deleted_at = ? WHERE id = ? AND deleted_at IS NULL;");
    stmt.bindInt64(1, toEpochMillis(when));
    stmt.bindInt64(2, id.value());
    stmt.run();
    return connection_.changes() > 0;
}

std::unique_ptr<TrackRepository> makeSqliteTrackRepository(const std::string& databasePath) {
    return std::make_unique<SqliteTrackRepository>(databasePath);
}

} // namespace musicbox::db

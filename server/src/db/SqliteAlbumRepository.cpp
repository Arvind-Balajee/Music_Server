#include "SqliteAlbumRepository.hpp"

#include "MigrationRunner.hpp"
#include "SqliteStatement.hpp"

namespace musicbox::db {

namespace {

musicbox::Album mapRow(SqliteStatement& stmt) {
    musicbox::Album album;
    album.id = musicbox::AlbumId(stmt.columnInt64(0));
    album.title = stmt.columnText(1);
    if (auto v = stmt.columnOptionalInt64(2)) {
        album.albumArtistId = musicbox::ArtistId(*v);
    }
    if (auto v = stmt.columnOptionalInt64(3)) {
        album.year = static_cast<int>(*v);
    }
    album.hasArtwork = stmt.columnInt64(4) != 0;
    return album;
}

} // namespace

SqliteAlbumRepository::SqliteAlbumRepository(const std::string& databasePath) : connection_(databasePath) { migrate(connection_); }

std::optional<musicbox::Album> SqliteAlbumRepository::findById(musicbox::AlbumId id) {
    SqliteStatement stmt(connection_, "SELECT id, title, album_artist_id, year, has_artwork FROM albums WHERE id = ?;");
    stmt.bindInt64(1, id.value());
    if (!stmt.step()) {
        return std::nullopt;
    }
    return mapRow(stmt);
}

std::vector<musicbox::Album> SqliteAlbumRepository::list(const AlbumQuery& query) {
    SqliteStatement stmt(connection_,
                          "SELECT id, title, album_artist_id, year, has_artwork FROM albums "
                          "WHERE (?1 IS NULL OR album_artist_id = ?1) "
                          "AND (?2 IS NULL OR title LIKE ?2 COLLATE NOCASE) "
                          "ORDER BY title COLLATE NOCASE LIMIT ?3 OFFSET ?4;");
    stmt.bindOptionalInt64(1, query.artistId.has_value() ? std::optional<std::int64_t>(query.artistId->value()) : std::nullopt);
    if (query.search.has_value()) {
        stmt.bindText(2, "%" + *query.search + "%");
    } else {
        stmt.bindNull(2);
    }
    stmt.bindInt64(3, static_cast<std::int64_t>(query.limit));
    stmt.bindInt64(4, static_cast<std::int64_t>(query.offset));

    std::vector<musicbox::Album> results;
    while (stmt.step()) {
        results.push_back(mapRow(stmt));
    }
    return results;
}

std::size_t SqliteAlbumRepository::count(const AlbumQuery& query) {
    SqliteStatement stmt(connection_,
                          "SELECT COUNT(*) FROM albums "
                          "WHERE (?1 IS NULL OR album_artist_id = ?1) "
                          "AND (?2 IS NULL OR title LIKE ?2 COLLATE NOCASE);");
    stmt.bindOptionalInt64(1, query.artistId.has_value() ? std::optional<std::int64_t>(query.artistId->value()) : std::nullopt);
    if (query.search.has_value()) {
        stmt.bindText(2, "%" + *query.search + "%");
    } else {
        stmt.bindNull(2);
    }
    stmt.step();
    return static_cast<std::size_t>(stmt.columnInt64(0));
}

std::unique_ptr<AlbumRepository> makeSqliteAlbumRepository(const std::string& databasePath) {
    return std::make_unique<SqliteAlbumRepository>(databasePath);
}

} // namespace musicbox::db

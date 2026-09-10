#include "SqliteArtistRepository.hpp"

#include "MigrationRunner.hpp"
#include "SqliteStatement.hpp"

namespace musicbox::db {

namespace {

musicbox::Artist mapRow(SqliteStatement& stmt) {
    musicbox::Artist artist;
    artist.id = musicbox::ArtistId(stmt.columnInt64(0));
    artist.name = stmt.columnText(1);
    artist.sortName = stmt.columnOptionalText(2);
    return artist;
}

} // namespace

SqliteArtistRepository::SqliteArtistRepository(const std::string& databasePath) : connection_(databasePath) { migrate(connection_); }

std::optional<musicbox::Artist> SqliteArtistRepository::findById(musicbox::ArtistId id) {
    SqliteStatement stmt(connection_, "SELECT id, name, sort_name FROM artists WHERE id = ?;");
    stmt.bindInt64(1, id.value());
    if (!stmt.step()) {
        return std::nullopt;
    }
    return mapRow(stmt);
}

std::vector<musicbox::Artist> SqliteArtistRepository::list(const ArtistQuery& query) {
    SqliteStatement stmt(connection_,
                          "SELECT id, name, sort_name FROM artists "
                          "WHERE (?1 IS NULL OR name LIKE ?1 COLLATE NOCASE) "
                          "ORDER BY name COLLATE NOCASE LIMIT ?2 OFFSET ?3;");
    if (query.search.has_value()) {
        stmt.bindText(1, "%" + *query.search + "%");
    } else {
        stmt.bindNull(1);
    }
    stmt.bindInt64(2, static_cast<std::int64_t>(query.limit));
    stmt.bindInt64(3, static_cast<std::int64_t>(query.offset));

    std::vector<musicbox::Artist> results;
    while (stmt.step()) {
        results.push_back(mapRow(stmt));
    }
    return results;
}

std::size_t SqliteArtistRepository::count(const ArtistQuery& query) {
    SqliteStatement stmt(connection_, "SELECT COUNT(*) FROM artists WHERE (?1 IS NULL OR name LIKE ?1 COLLATE NOCASE);");
    if (query.search.has_value()) {
        stmt.bindText(1, "%" + *query.search + "%");
    } else {
        stmt.bindNull(1);
    }
    stmt.step();
    return static_cast<std::size_t>(stmt.columnInt64(0));
}

std::unique_ptr<ArtistRepository> makeSqliteArtistRepository(const std::string& databasePath) {
    return std::make_unique<SqliteArtistRepository>(databasePath);
}

} // namespace musicbox::db

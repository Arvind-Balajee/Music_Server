#include "SqliteLibraryRootRepository.hpp"

#include "MigrationRunner.hpp"
#include "SqliteStatement.hpp"
#include "TimeUtil.hpp"

namespace musicbox::db {

namespace {

musicbox::LibraryRoot mapRow(SqliteStatement& stmt) {
    musicbox::LibraryRoot root;
    root.id = musicbox::LibraryRootId(stmt.columnInt64(0));
    root.absolutePath = stmt.columnText(1);
    root.addedAt = fromEpochMillis(stmt.columnInt64(2));
    root.lastScanAt = fromOptionalEpochMillis(stmt.columnOptionalInt64(3));
    return root;
}

} // namespace

SqliteLibraryRootRepository::SqliteLibraryRootRepository(const std::string& databasePath) : connection_(databasePath) {
    migrate(connection_);
}

std::vector<musicbox::LibraryRoot> SqliteLibraryRootRepository::list() {
    SqliteStatement stmt(connection_, "SELECT id, path, added_at, last_scan_at FROM library_roots ORDER BY id;");
    std::vector<musicbox::LibraryRoot> results;
    while (stmt.step()) {
        results.push_back(mapRow(stmt));
    }
    return results;
}

musicbox::LibraryRoot SqliteLibraryRootRepository::upsert(const std::string& absolutePath) {
    SqliteTransaction transaction(connection_);

    SqliteStatement select(connection_, "SELECT id, path, added_at, last_scan_at FROM library_roots WHERE path = ?;");
    select.bindText(1, absolutePath);
    if (select.step()) {
        auto root = mapRow(select);
        // Must reset before COMMIT -- SQLite refuses to commit while any
        // statement on the connection is still mid-result.
        select.reset();
        transaction.commit();
        return root;
    }

    const auto now = std::chrono::system_clock::now();
    SqliteStatement insert(connection_, "INSERT INTO library_roots(path, added_at, last_scan_at) VALUES(?, ?, NULL);");
    insert.bindText(1, absolutePath);
    insert.bindInt64(2, toEpochMillis(now));
    insert.run();

    musicbox::LibraryRoot root;
    root.id = musicbox::LibraryRootId(connection_.lastInsertRowId());
    root.absolutePath = absolutePath;
    root.addedAt = now;
    root.lastScanAt = std::nullopt;

    transaction.commit();
    return root;
}

void SqliteLibraryRootRepository::markScanned(musicbox::LibraryRootId id, std::chrono::system_clock::time_point when) {
    SqliteStatement stmt(connection_, "UPDATE library_roots SET last_scan_at = ? WHERE id = ?;");
    stmt.bindInt64(1, toEpochMillis(when));
    stmt.bindInt64(2, id.value());
    stmt.run();
}

std::unique_ptr<LibraryRootRepository> makeSqliteLibraryRootRepository(const std::string& databasePath) {
    return std::make_unique<SqliteLibraryRootRepository>(databasePath);
}

} // namespace musicbox::db

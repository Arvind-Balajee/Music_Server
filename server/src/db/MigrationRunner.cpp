#include "MigrationRunner.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "SqliteStatement.hpp"

namespace musicbox::db {

namespace {

std::int64_t nowEpochMillis() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// Returns the numeric prefix of a migration filename ("0001_init.sql" -> 1), or
// std::nullopt if the filename doesn't start with digits followed by '_'.
std::optional<std::int64_t> parseVersionPrefix(const std::string& stem) {
    std::size_t i = 0;
    while (i < stem.size() && std::isdigit(static_cast<unsigned char>(stem[i])) != 0) {
        ++i;
    }
    if (i == 0 || i >= stem.size() || stem[i] != '_') {
        return std::nullopt;
    }
    try {
        return std::stoll(stem.substr(0, i));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open migration file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::int64_t currentSchemaVersion(SqliteConnection& connection) {
    SqliteStatement checkTable(connection,
                                "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='schema_migrations';");
    checkTable.step();
    const bool tableExists = checkTable.columnInt64(0) > 0;
    if (!tableExists) {
        return 0;
    }

    SqliteStatement maxVersion(connection, "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;");
    maxVersion.step();
    return maxVersion.columnInt64(0);
}

} // namespace

std::vector<Migration> loadMigrations(const std::filesystem::path& directory) {
    std::vector<Migration> migrations;

    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error("migrations directory does not exist: " + directory.string());
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".sql") {
            continue;
        }
        const std::string stem = entry.path().stem().string();
        const auto version = parseVersionPrefix(stem);
        if (!version.has_value()) {
            throw std::runtime_error("migration filename does not match '<version>_<name>.sql': " + entry.path().string());
        }
        migrations.push_back(Migration{*version, stem, readFile(entry.path())});
    }

    std::sort(migrations.begin(), migrations.end(), [](const Migration& a, const Migration& b) { return a.version < b.version; });
    return migrations;
}

std::filesystem::path defaultMigrationsDirectory() { return std::filesystem::path(__FILE__).parent_path() / "migrations"; }

void applyMigrations(SqliteConnection& connection, const std::vector<Migration>& migrations) {
    std::vector<Migration> sorted = migrations;
    std::sort(sorted.begin(), sorted.end(), [](const Migration& a, const Migration& b) { return a.version < b.version; });

    for (const auto& migration : sorted) {
        SqliteTransaction transaction(connection);

        // Re-check inside the write lock: another connection to the same
        // database may have applied this migration between our caller
        // deciding to run and us acquiring the lock (docs/database.md "one
        // connection per repository instance" -- multiple repositories can
        // race to migrate a freshly-created database at startup).
        if (migration.version <= currentSchemaVersion(connection)) {
            continue; // transaction destructor rolls back the no-op BEGIN
        }

        connection.exec(migration.sql);

        SqliteStatement record(connection, "INSERT INTO schema_migrations(version, applied_at) VALUES(?, ?);");
        record.bindInt64(1, migration.version);
        record.bindInt64(2, nowEpochMillis());
        record.run();

        transaction.commit();
    }
}

void migrate(SqliteConnection& connection, const std::filesystem::path& directory) { applyMigrations(connection, loadMigrations(directory)); }

} // namespace musicbox::db

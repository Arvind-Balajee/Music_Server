#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "SqliteConnection.hpp"

namespace musicbox::db {

struct Migration {
    std::int64_t version = 0;
    std::string name;
    std::string sql;
};

// Reads every "<version>_<name>.sql" file directly inside `directory`
// (non-recursive), sorted ascending by the numeric version prefix. Throws
// std::runtime_error if a filename doesn't match the expected pattern.
[[nodiscard]] std::vector<Migration> loadMigrations(const std::filesystem::path& directory);

// The directory this binary was built with migrations in (server/src/db/migrations
// at the time this translation unit was compiled). Works for local dev builds and
// CI (a fresh checkout each time); a packaged/installed deployment should pass an
// explicit directory instead (see migrate() below) -- see the "Known limitations"
// note in this agent's final report.
[[nodiscard]] std::filesystem::path defaultMigrationsDirectory();

// Applies every migration in `migrations` whose version is not already present in
// schema_migrations, in ascending version order, each inside its own transaction,
// recording {version, applied_at} on success. Safe to call from multiple
// SqliteConnections attached to the same underlying database (e.g. one per
// repository instance, docs/database.md): uses BEGIN IMMEDIATE + a post-lock
// re-check of the current version to avoid double-applying under a startup race.
void applyMigrations(SqliteConnection& connection, const std::vector<Migration>& migrations);

// Convenience wrapper: loadMigrations(directory) then applyMigrations(connection, ...).
void migrate(SqliteConnection& connection,
             const std::filesystem::path& directory = defaultMigrationsDirectory());

} // namespace musicbox::db

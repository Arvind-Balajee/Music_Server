#pragma once

#include <cstdint>
#include <string>

struct sqlite3;

namespace musicbox::db {

// RAII wrapper around a single sqlite3* connection.
//
// Concurrency contract (docs/architecture.md §4, docs/database.md,
// docs/adr/0002-use-sqlite.md): exactly one SqliteConnection per repository
// instance, and repository instances are never shared across threads. This
// class enforces the "one connection" half of that by owning exactly one
// sqlite3* for its lifetime; the "never shared across threads" half is a
// caller contract (a plain sqlite3* is not itself thread-affine, but WAL-mode
// SQLite still serializes writers, and this project's threading model assumes
// each worker thread owns its own repository/connection rather than locking a
// shared one on every query).
//
// A path of the form "file:...?...&cache=shared" (SQLite's URI syntax) opens
// the connection in URI mode, which is how tests attach multiple independent
// SqliteConnection instances to one shared-cache in-memory database to
// exercise multiple repositories against the same logical DB without a real
// file on disk.
class SqliteConnection {
public:
    explicit SqliteConnection(const std::string& path);
    ~SqliteConnection();

    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    SqliteConnection(SqliteConnection&& other) noexcept;
    SqliteConnection& operator=(SqliteConnection&& other) noexcept;

    // Raw handle for building prepared statements (see SqliteStatement).
    [[nodiscard]] sqlite3* handle() const noexcept { return db_; }

    // Executes one or more semicolon-separated statements with no bound
    // parameters and no result set (DDL, PRAGMAs, transaction control).
    // Throws std::runtime_error on failure.
    void exec(const std::string& sql);

    void beginImmediate();
    void commit();
    void rollback();

    [[nodiscard]] std::int64_t lastInsertRowId() const noexcept;
    [[nodiscard]] int changes() const noexcept;

private:
    void close() noexcept;

    sqlite3* db_ = nullptr;
};

// RAII "BEGIN IMMEDIATE ... COMMIT/ROLLBACK" guard: rolls back automatically on
// destruction unless commit() was called, so a thrown exception anywhere
// between construction and commit() leaves the connection outside a
// transaction rather than leaking one.
class SqliteTransaction {
public:
    explicit SqliteTransaction(SqliteConnection& connection);
    ~SqliteTransaction();

    SqliteTransaction(const SqliteTransaction&) = delete;
    SqliteTransaction& operator=(const SqliteTransaction&) = delete;
    SqliteTransaction(SqliteTransaction&&) = delete;
    SqliteTransaction& operator=(SqliteTransaction&&) = delete;

    void commit();

private:
    SqliteConnection& connection_;
    bool active_ = true;
};

} // namespace musicbox::db

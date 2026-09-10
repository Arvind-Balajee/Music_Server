#include "SqliteConnection.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace musicbox::db {

namespace {

int openFlagsFor(const std::string& path) {
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (path.rfind("file:", 0) == 0) {
        flags |= SQLITE_OPEN_URI;
    }
    return flags;
}

} // namespace

SqliteConnection::SqliteConnection(const std::string& path) {
    const int rc = sqlite3_open_v2(path.c_str(), &db_, openFlagsFor(path), nullptr);
    if (rc != SQLITE_OK) {
        std::string message = "sqlite3_open_v2 failed for '" + path + "': " +
                               (db_ != nullptr ? sqlite3_errmsg(db_) : sqlite3_errstr(rc));
        if (db_ != nullptr) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        throw std::runtime_error(message);
    }

    // Concurrency posture per docs/adr/0002-use-sqlite.md: WAL lets the
    // scanner (writer) and API handlers (readers) proceed concurrently. WAL is
    // a no-op fallback (silently stays in "memory"/rollback mode) for
    // in-memory databases, which is fine for tests.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec("PRAGMA busy_timeout=5000;");
}

SqliteConnection::~SqliteConnection() { close(); }

SqliteConnection::SqliteConnection(SqliteConnection&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

SqliteConnection& SqliteConnection::operator=(SqliteConnection&& other) noexcept {
    if (this != &other) {
        close();
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

void SqliteConnection::close() noexcept {
    if (db_ != nullptr) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

void SqliteConnection::exec(const std::string& sql) {
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string message = "sqlite3_exec failed: ";
        message += (errMsg != nullptr ? errMsg : sqlite3_errmsg(db_));
        sqlite3_free(errMsg);
        throw std::runtime_error(message);
    }
}

void SqliteConnection::beginImmediate() { exec("BEGIN IMMEDIATE;"); }

void SqliteConnection::commit() { exec("COMMIT;"); }

void SqliteConnection::rollback() { exec("ROLLBACK;"); }

std::int64_t SqliteConnection::lastInsertRowId() const noexcept {
    return static_cast<std::int64_t>(sqlite3_last_insert_rowid(db_));
}

int SqliteConnection::changes() const noexcept { return sqlite3_changes(db_); }

SqliteTransaction::SqliteTransaction(SqliteConnection& connection) : connection_(connection) { connection_.beginImmediate(); }

SqliteTransaction::~SqliteTransaction() {
    if (active_) {
        try {
            connection_.rollback();
        } catch (...) {
            // Destructors must not throw; a failed rollback here means the
            // connection is already in a bad state that a subsequent
            // operation will surface.
        }
    }
}

void SqliteTransaction::commit() {
    connection_.commit();
    active_ = false;
}

} // namespace musicbox::db

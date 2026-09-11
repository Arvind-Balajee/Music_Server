#include "SqliteStatement.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace musicbox::db {

namespace {

[[noreturn]] void throwSqliteError(sqlite3* db, const std::string& what) {
    throw std::runtime_error(what + ": " + (db != nullptr ? sqlite3_errmsg(db) : "unknown error"));
}

} // namespace

SqliteStatement::SqliteStatement(const SqliteConnection& connection, const std::string& sql)
    : db_(connection.handle()) {
    const int rc =
        sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()) + 1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throwSqliteError(db_, "sqlite3_prepare_v2 failed for '" + sql + "'");
    }
}

SqliteStatement::~SqliteStatement() {
    if (stmt_ != nullptr) {
        sqlite3_finalize(stmt_);
    }
}

void SqliteStatement::bindInt64(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, static_cast<sqlite3_int64>(value)) != SQLITE_OK) {
        throwSqliteError(db_, "sqlite3_bind_int64 failed");
    }
}

void SqliteStatement::bindOptionalInt64(int index, std::optional<std::int64_t> value) {
    if (value.has_value()) {
        bindInt64(index, *value);
    } else {
        bindNull(index);
    }
}

void SqliteStatement::bindText(int index, const std::string& value) {
    if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throwSqliteError(db_, "sqlite3_bind_text failed");
    }
}

void SqliteStatement::bindOptionalText(int index, const std::optional<std::string>& value) {
    if (value.has_value()) {
        bindText(index, *value);
    } else {
        bindNull(index);
    }
}

void SqliteStatement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        throwSqliteError(db_, "sqlite3_bind_null failed");
    }
}

bool SqliteStatement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throwSqliteError(db_, "sqlite3_step failed");
}

void SqliteStatement::run() {
    while (step()) {
        // Drain any (unexpected) result rows; callers using run() don't want them.
    }
}

void SqliteStatement::reset() {
    if (sqlite3_reset(stmt_) != SQLITE_OK) {
        throwSqliteError(db_, "sqlite3_reset failed");
    }
}

void SqliteStatement::clearBindings() {
    sqlite3_clear_bindings(stmt_);
}

std::int64_t SqliteStatement::columnInt64(int index) const {
    return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, index));
}

std::optional<std::int64_t> SqliteStatement::columnOptionalInt64(int index) const {
    if (isColumnNull(index)) {
        return std::nullopt;
    }
    return columnInt64(index);
}

std::string SqliteStatement::columnText(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    const int bytes = sqlite3_column_bytes(stmt_, index);
    if (text == nullptr) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(bytes));
}

std::optional<std::string> SqliteStatement::columnOptionalText(int index) const {
    if (isColumnNull(index)) {
        return std::nullopt;
    }
    return columnText(index);
}

bool SqliteStatement::isColumnNull(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

} // namespace musicbox::db

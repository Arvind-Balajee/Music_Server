#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "SqliteConnection.hpp"

struct sqlite3_stmt;

namespace musicbox::db {

// RAII wrapper around a prepared statement. Not copyable or shareable across
// threads (it holds a pointer bound to one SqliteConnection's sqlite3*).
// Typical use:
//
//   SqliteStatement stmt(connection, "SELECT title FROM tracks WHERE id = ?");
//   stmt.bindInt64(1, id.value());
//   if (stmt.step()) {
//       auto title = stmt.columnText(0);
//   }
class SqliteStatement {
public:
    SqliteStatement(const SqliteConnection& connection, const std::string& sql);
    ~SqliteStatement();

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;
    SqliteStatement(SqliteStatement&&) = delete;
    SqliteStatement& operator=(SqliteStatement&&) = delete;

    // 1-based bind indices, matching sqlite3_bind_*.
    void bindInt64(int index, std::int64_t value);
    void bindOptionalInt64(int index, std::optional<std::int64_t> value);
    void bindText(int index, const std::string& value);
    void bindOptionalText(int index, const std::optional<std::string>& value);
    void bindNull(int index);

    // Advances to the next row. Returns true if a row is available (SQLITE_ROW),
    // false at the end of the result set (SQLITE_DONE). Throws on error.
    bool step();

    // Runs the statement to completion for statements with no result rows
    // (INSERT/UPDATE/DELETE without RETURNING).
    void run();

    // Resets the statement so it can be re-executed (bindings must be re-applied
    // unless bindings are unchanged; SQLite clears bindings only if
    // clearBindings() below is called).
    void reset();
    void clearBindings();

    // 0-based column indices, matching sqlite3_column_*. Valid only after step()
    // returned true.
    [[nodiscard]] std::int64_t columnInt64(int index) const;
    [[nodiscard]] std::optional<std::int64_t> columnOptionalInt64(int index) const;
    [[nodiscard]] std::string columnText(int index) const;
    [[nodiscard]] std::optional<std::string> columnOptionalText(int index) const;
    [[nodiscard]] bool isColumnNull(int index) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;
};

} // namespace musicbox::db

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace wt {

// Throws ApiError describing the most recent SQLite error on `db`.
[[noreturn]] void throw_sqlite_error(sqlite3* db, std::string_view context);
[[noreturn]] void throw_sqlite_error(sqlite3_stmt* stmt, std::string_view context);

// Thin RAII wrapper around a sqlite3 connection.
class Database {
 public:
  Database() = default;
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

  // Opens (and creates, including parent directories) the database file and
  // applies the required PRAGMAs. Throws on failure.
  void open(const std::string& path, int busy_timeout_ms = 5000, bool wal = true);
  void close();

  bool is_open() const noexcept { return handle_ != nullptr; }
  sqlite3* handle() const noexcept { return handle_; }

  // Executes one or more SQL statements without parameters. For migrations and
  // DDL only.
  void exec(std::string_view sql);

  std::int64_t last_insert_rowid() const;
  int changes() const;

 private:
  sqlite3* handle_ = nullptr;
};

}  // namespace wt

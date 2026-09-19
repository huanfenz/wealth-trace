#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace wt {

class Database;

// RAII wrapper around a prepared statement. Binding is 1-based, matching the
// SQLite API. User input must only ever be supplied through bind().
class Statement {
 public:
  Statement(Database& database, std::string_view sql);
  ~Statement();

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&& other) noexcept;
  Statement& operator=(Statement&& other) noexcept;

  Statement& bind(int index, std::int64_t value);
  Statement& bind(int index, int value);
  Statement& bind(int index, bool value);
  Statement& bind(int index, const std::string& value);
  Statement& bind(int index, std::string_view value);
  // Prevents string literals from decaying to bool via pointer-to-bool.
  Statement& bind(int index, const char* value) {
    return bind(index, std::string_view(value != nullptr ? value : ""));
  }
  Statement& bind_null(int index);
  Statement& bind_optional_int64(int index, const std::optional<std::int64_t>& value);
  Statement& bind_optional_text(int index, const std::optional<std::string>& value);
  Statement& bind_optional_bool(int index, const std::optional<bool>& value);

  // Advances one row. Returns true when a row is available.
  bool step();

  // Executes a statement that returns no rows (INSERT/UPDATE/DELETE).
  void run();

  void reset();

  bool is_null(int column) const;
  std::int64_t get_int64(int column) const;
  int get_int(int column) const;
  bool get_bool(int column) const;
  double get_double(int column) const;
  std::string get_text(int column) const;
  std::string get_text_or(int column, std::string fallback) const;
  std::optional<std::int64_t> get_optional_int64(int column) const;
  std::optional<std::string> get_optional_text(int column) const;
  std::optional<bool> get_optional_bool(int column) const;

  sqlite3_stmt* handle() const noexcept { return stmt_; }

 private:
  sqlite3* database_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
  std::string sql_;
};

}  // namespace wt

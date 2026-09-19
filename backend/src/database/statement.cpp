#include "database/statement.hpp"

#include <string>
#include <utility>

#include <sqlite3.h>

#include "common/error.hpp"
#include "database/database.hpp"

namespace wt {

Statement::Statement(Database& database, std::string_view sql)
    : database_(database.handle()), sql_(sql) {
  if (database_ == nullptr) {
    throw database_error("database is not open");
  }
  const int code = sqlite3_prepare_v2(database_, sql_.c_str(), -1, &stmt_, nullptr);
  if (code != SQLITE_OK) {
    throw_sqlite_error(database_, "failed to prepare statement");
  }
}

Statement::~Statement() {
  if (stmt_ != nullptr) {
    sqlite3_finalize(stmt_);
  }
}

Statement::Statement(Statement&& other) noexcept
    : database_(other.database_), stmt_(other.stmt_), sql_(std::move(other.sql_)) {
  other.stmt_ = nullptr;
  other.database_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
  if (this != &other) {
    if (stmt_ != nullptr) {
      sqlite3_finalize(stmt_);
    }
    database_ = other.database_;
    stmt_ = other.stmt_;
    sql_ = std::move(other.sql_);
    other.stmt_ = nullptr;
    other.database_ = nullptr;
  }
  return *this;
}

Statement& Statement::bind(int index, std::int64_t value) {
  if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind integer");
  }
  return *this;
}

Statement& Statement::bind(int index, int value) {
  return bind(index, static_cast<std::int64_t>(value));
}

Statement& Statement::bind(int index, bool value) {
  return bind(index, static_cast<std::int64_t>(value ? 1 : 0));
}

Statement& Statement::bind(int index, const std::string& value) {
  return bind(index, std::string_view(value));
}

Statement& Statement::bind(int index, std::string_view value) {
  if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind text");
  }
  return *this;
}

Statement& Statement::bind_null(int index) {
  if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind null");
  }
  return *this;
}

Statement& Statement::bind_optional_int64(int index,
                                          const std::optional<std::int64_t>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

Statement& Statement::bind_optional_text(int index,
                                         const std::optional<std::string>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

Statement& Statement::bind_optional_bool(int index, const std::optional<bool>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

bool Statement::step() {
  const int code = sqlite3_step(stmt_);
  if (code == SQLITE_ROW) {
    return true;
  }
  if (code == SQLITE_DONE) {
    return false;
  }
  throw_sqlite_error(stmt_, "failed to execute statement");
}

void Statement::run() {
  while (step()) {
  }
}

void Statement::reset() {
  sqlite3_reset(stmt_);
  sqlite3_clear_bindings(stmt_);
}

bool Statement::is_null(int column) const {
  return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

std::int64_t Statement::get_int64(int column) const {
  return sqlite3_column_int64(stmt_, column);
}

int Statement::get_int(int column) const {
  return static_cast<int>(sqlite3_column_int64(stmt_, column));
}

bool Statement::get_bool(int column) const {
  return sqlite3_column_int64(stmt_, column) != 0;
}

double Statement::get_double(int column) const {
  return sqlite3_column_double(stmt_, column);
}

std::string Statement::get_text(int column) const {
  const unsigned char* text = sqlite3_column_text(stmt_, column);
  if (text == nullptr) {
    return {};
  }
  const int size = sqlite3_column_bytes(stmt_, column);
  return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
}

std::string Statement::get_text_or(int column, std::string fallback) const {
  return is_null(column) ? std::move(fallback) : get_text(column);
}

std::optional<std::int64_t> Statement::get_optional_int64(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_int64(column);
}

std::optional<std::string> Statement::get_optional_text(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_text(column);
}

std::optional<bool> Statement::get_optional_bool(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_bool(column);
}

}  // namespace wt

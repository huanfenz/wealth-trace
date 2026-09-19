#include "database/database.hpp"

#include <filesystem>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "common/error.hpp"

namespace wt {

void throw_sqlite_error(sqlite3* db, std::string_view context) {
  const int code = db != nullptr ? sqlite3_errcode(db) : SQLITE_ERROR;
  const char* message = db != nullptr ? sqlite3_errmsg(db) : "unknown database error";
  std::string text = std::string(context) + ": " + message;
  if (code == SQLITE_CONSTRAINT) {
    throw conflict(text);
  }
  if (code == SQLITE_BUSY || code == SQLITE_LOCKED) {
    throw ApiError(error_code::kDatabase, 503, text);
  }
  throw database_error(text);
}

void throw_sqlite_error(sqlite3_stmt* stmt, std::string_view context) {
  throw_sqlite_error(stmt != nullptr ? sqlite3_db_handle(stmt) : nullptr, context);
}

Database::~Database() { close(); }

Database::Database(Database&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

void Database::open(const std::string& path, int busy_timeout_ms, bool wal) {
  close();
  if (path != ":memory:") {
    const std::filesystem::path file_path(path);
    if (file_path.has_parent_path()) {
      std::error_code error;
      std::filesystem::create_directories(file_path.parent_path(), error);
      if (error) {
        throw database_error("failed to create database directory: " +
                             file_path.parent_path().string() + ": " + error.message());
      }
    }
  }

  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
  if (sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
    const std::string message =
        handle_ != nullptr ? sqlite3_errmsg(handle_) : "unable to open database";
    if (handle_ != nullptr) {
      sqlite3_close(handle_);
      handle_ = nullptr;
    }
    throw database_error("failed to open database '" + path + "': " + message);
  }

  sqlite3_busy_timeout(handle_, busy_timeout_ms);
  exec("PRAGMA foreign_keys = ON;");
  if (wal) {
    exec("PRAGMA journal_mode = WAL;");
  }
  exec("PRAGMA synchronous = NORMAL;");
}

void Database::close() {
  if (handle_ != nullptr) {
    sqlite3_close(handle_);
    handle_ = nullptr;
  }
}

void Database::exec(std::string_view sql) {
  if (handle_ == nullptr) {
    throw database_error("database is not open");
  }
  char* error_message = nullptr;
  const std::string statement(sql);
  const int code = sqlite3_exec(handle_, statement.c_str(), nullptr, nullptr, &error_message);
  if (code != SQLITE_OK) {
    std::string message = error_message != nullptr ? error_message : sqlite3_errmsg(handle_);
    sqlite3_free(error_message);
    if (code == SQLITE_CONSTRAINT) {
      throw conflict("database constraint failed: " + message);
    }
    throw database_error("SQL execution failed: " + message);
  }
}

std::int64_t Database::last_insert_rowid() const {
  if (handle_ == nullptr) {
    throw database_error("database is not open");
  }
  return sqlite3_last_insert_rowid(handle_);
}

int Database::changes() const {
  if (handle_ == nullptr) {
    throw database_error("database is not open");
  }
  return sqlite3_changes(handle_);
}

}  // namespace wt

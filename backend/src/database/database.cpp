#include "database/database.hpp"

// 数据库连接实现：打开/关闭文件、应用 PRAGMA，并把 SQLite 错误映射为 ApiError。

#include <filesystem>
#include <string>
#include <utility>

#include <sqlite3.h>

#include "common/error.hpp"

namespace wt {

void throw_sqlite_error(sqlite3* db, std::string_view context) {
  // 连接尚未建立时退回到通用错误码，避免解引用空指针。
  const int code = db != nullptr ? sqlite3_errcode(db) : SQLITE_ERROR;
  const char* message = db != nullptr ? sqlite3_errmsg(db) : "unknown database error";
  std::string text = std::string(context) + ": " + message;
  // SQLITE_CONSTRAINT 视为业务冲突（唯一键/外键/检查约束），映射为 HTTP 409。
  if (code == SQLITE_CONSTRAINT) {
    throw conflict(text);
  }
  // BUSY/LOCKED 表示瞬时锁竞争，调用方稍后可重试，因此返回 503 而非 500。
  if (code == SQLITE_BUSY || code == SQLITE_LOCKED) {
    throw ApiError(error_code::kDatabase, 503, text);
  }
  throw database_error(text);
}

void throw_sqlite_error(sqlite3_stmt* stmt, std::string_view context) {
  // 语句句柄本身不暴露错误码，需反查其所属连接后复用上面的映射。
  throw_sqlite_error(stmt != nullptr ? sqlite3_db_handle(stmt) : nullptr, context);
}

// RAII：析构时关闭连接。
Database::~Database() { close(); }

// 移动构造：直接窃取句柄并把源置空，避免源析构时重复关闭。
Database::Database(Database&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

// 移动赋值：先释放自身句柄，再窃取源句柄（含自赋值判断）。
Database& Database::operator=(Database&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

void Database::open(const std::string& path, int busy_timeout_ms, bool wal) {
  // 先关掉可能已存在的连接，保证 open 可重复调用。
  close();
  // ":memory:" 是内存库，没有父目录；文件库需要先保证父目录存在。
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

  // FULLMUTEX 允许同一连接在多线程下被互斥使用；READWRITE|CREATE 表示读写并自动建库。
  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
  if (sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
    const std::string message =
        handle_ != nullptr ? sqlite3_errmsg(handle_) : "unable to open database";
    // 打开失败时 sqlite3_open_v2 仍可能返回句柄，需要显式关闭并置空，避免泄漏。
    if (handle_ != nullptr) {
      sqlite3_close(handle_);
      handle_ = nullptr;
    }
    throw database_error("failed to open database '" + path + "': " + message);
  }

  // 设置锁等待超时，缓解并发写时的 SQLITE_BUSY。
  sqlite3_busy_timeout(handle_, busy_timeout_ms);
  // SQLite 外键约束默认关闭，必须显式开启。
  exec("PRAGMA foreign_keys = ON;");
  if (wal) {
    // WAL 允许读写并发，提升多连接场景的吞吐。
    exec("PRAGMA journal_mode = WAL;");
  }
  // NORMAL 在 WAL 模式下兼顾安全与性能。
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
    // sqlite3_exec 分配的错误字符串需由调用方释放。
    sqlite3_free(error_message);
    // 约束失败单独映射为业务冲突。
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

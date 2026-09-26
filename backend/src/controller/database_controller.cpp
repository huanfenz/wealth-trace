#include "controller/database_controller.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <sqlite3.h>

#include "common/error.hpp"
#include "common/logging.hpp"
#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"

namespace wt {
namespace {

namespace fs = std::filesystem;
constexpr std::size_t kMaxImportBytes = 100U * 1024U * 1024U;
std::atomic_uint64_t temp_sequence{0};

std::string unique_suffix() {
  const auto now = std::chrono::system_clock::now().time_since_epoch().count();
  return std::to_string(now) + "-" +
         std::to_string(temp_sequence.fetch_add(1, std::memory_order_relaxed));
}

class TempFile {
 public:
  explicit TempFile(fs::path path) : path_(std::move(path)) {}
  ~TempFile() {
    if (!remove_) return;
    std::error_code ignored;
    fs::remove(path_, ignored);
    fs::remove(path_.string() + "-wal", ignored);
    fs::remove(path_.string() + "-shm", ignored);
  }
  const fs::path& path() const { return path_; }
  void keep() { remove_ = false; }

 private:
  fs::path path_;
  bool remove_ = true;
};

void backup_database(sqlite3* source, sqlite3* destination) {
  sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
  if (backup == nullptr) {
    throw database_error(std::string("failed to initialize SQLite backup: ") +
                         sqlite3_errmsg(destination));
  }

  int result = SQLITE_OK;
  do {
    result = sqlite3_backup_step(backup, 256);
    if (result == SQLITE_BUSY || result == SQLITE_LOCKED) {
      sqlite3_sleep(10);
    }
  } while (result == SQLITE_OK || result == SQLITE_BUSY || result == SQLITE_LOCKED);

  const int finish_result = sqlite3_backup_finish(backup);
  if (result != SQLITE_DONE || finish_result != SQLITE_OK) {
    const int code = result != SQLITE_DONE ? result : finish_result;
    throw database_error(std::string("SQLite backup failed: ") + sqlite3_errstr(code));
  }
}

class SqliteHandle {
 public:
  explicit SqliteHandle(const fs::path& path) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int result = sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr);
    if (result != SQLITE_OK) {
      const std::string message = handle_ != nullptr ? sqlite3_errmsg(handle_)
                                                    : sqlite3_errstr(result);
      if (handle_ != nullptr) sqlite3_close(handle_);
      handle_ = nullptr;
      throw database_error("failed to open backup file: " + message);
    }
  }
  ~SqliteHandle() {
    if (handle_ != nullptr) sqlite3_close(handle_);
  }
  SqliteHandle(const SqliteHandle&) = delete;
  SqliteHandle& operator=(const SqliteHandle&) = delete;
  sqlite3* get() const { return handle_; }
  void close() {
    if (handle_ != nullptr) {
      sqlite3_close(handle_);
      handle_ = nullptr;
    }
  }

 private:
  sqlite3* handle_ = nullptr;
};

void write_upload(const fs::path& path, const std::string& body) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw database_error("failed to create temporary database file");
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::replace);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
  output.close();
  if (!output) throw database_error("failed to write temporary database file");
}

void create_private_database_file(const fs::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw database_error("failed to create database snapshot file");
  output.close();
  if (!output) throw database_error("failed to initialize database snapshot file");
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::replace);
}

void check_database(sqlite3* db) {
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(db, "PRAGMA quick_check;", -1, &statement, nullptr) != SQLITE_OK) {
    throw invalid_request("上传文件不是有效的 SQLite 数据库");
  }
  const int check_result = sqlite3_step(statement);
  const bool integrity_ok = check_result == SQLITE_ROW &&
                            std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) == "ok" &&
                            sqlite3_step(statement) == SQLITE_DONE;
  sqlite3_finalize(statement);
  if (!integrity_ok) throw invalid_request("数据库完整性检查失败，未执行导入");

  if (sqlite3_prepare_v2(db, "PRAGMA foreign_key_check;", -1, &statement, nullptr) != SQLITE_OK) {
    throw invalid_request("无法检查数据库外键");
  }
  const int foreign_result = sqlite3_step(statement);
  sqlite3_finalize(statement);
  if (foreign_result != SQLITE_DONE) throw invalid_request("数据库存在无效的外键引用");

  if (sqlite3_prepare_v2(db,
                         "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                         "AND name='schema_migration';",
                         -1, &statement, nullptr) != SQLITE_OK) {
    throw invalid_request("上传文件不包含财迹数据库迁移记录");
  }
  const bool has_migrations = sqlite3_step(statement) == SQLITE_ROW &&
                              sqlite3_column_int(statement, 0) == 1;
  sqlite3_finalize(statement);
  if (!has_migrations) throw invalid_request("上传文件不包含财迹数据库迁移记录");

  if (sqlite3_prepare_v2(
          db,
          "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
          "('schema_migration','household','household_member','account','asset',"
          "'transaction','system_state');",
          -1, &statement, nullptr) != SQLITE_OK) {
    throw invalid_request("上传文件不是受支持的财迹数据库");
  }
  const bool has_core_schema = sqlite3_step(statement) == SQLITE_ROW &&
                               sqlite3_column_int(statement, 0) == 7;
  sqlite3_finalize(statement);
  if (!has_core_schema) throw invalid_request("上传文件不是受支持的财迹数据库");
}

}  // namespace

void DatabaseController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/database/export").methods("GET"_method)([this] {
    try {
      std::scoped_lock lock(database_.mutex());
      const fs::path parent = fs::path(database_path_).parent_path();
      const fs::path snapshot = (parent.empty() ? fs::path(".") : parent) /
                                ("wealth-trace-export-" + unique_suffix() + ".db");
      TempFile cleanup(snapshot);
      create_private_database_file(snapshot);
      SqliteHandle destination(snapshot);
      backup_database(database_.handle(), destination.get());
      destination.close();

      std::ifstream input(snapshot, std::ios::binary);
      if (!input) throw database_error("failed to read database export snapshot");
      std::string bytes((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
      crow::response response(200, std::move(bytes));
      response.set_header("Content-Type", "application/vnd.sqlite3");
      response.set_header("Content-Disposition", "attachment; filename=wealth-trace.db");
      response.set_header("Access-Control-Allow-Origin", "*");
      return response;
    } catch (const ApiError& error) {
      return http::fail(error.http_status(), error.code(), error.what());
    } catch (const std::exception& error) {
      log_error(std::string("database export failed: ") + error.what());
      return http::fail(500, error_code::kInternal, "database export failed");
    }
  });

  CROW_ROUTE(app, "/api/database/import").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          if (request.body.empty()) throw invalid_request("请选择数据库备份文件");
          if (request.body.size() > kMaxImportBytes) {
            throw ApiError(error_code::kInvalidRequest, 413,
                           "数据库文件不能超过 100 MiB");
          }

          const fs::path parent = fs::path(database_path_).parent_path();
          const fs::path directory = parent.empty() ? fs::path(".") : parent;
          std::error_code fs_error;
          fs::create_directories(directory, fs_error);
          if (fs_error) throw database_error("failed to create database directory");

          TempFile staged_path(directory / ("wealth-trace-import-" + unique_suffix() + ".db"));
          write_upload(staged_path.path(), request.body);
          Database staged;
          try {
            staged.open(staged_path.path().string());
          } catch (const std::exception&) {
            throw invalid_request("上传文件无法作为 SQLite 数据库打开");
          }
          check_database(staged.handle());
          MigrationRunner staged_migrations(staged);
          const auto staged_version = staged_migrations.current_version();

          std::scoped_lock lock(database_.mutex());
          MigrationRunner current_migrations(database_);
          const auto current_version = current_migrations.current_version();
          if (staged_version <= 0 || staged_version > current_version) {
            throw invalid_request("备份版本与当前程序不兼容");
          }

          staged_migrations.run(migrations_dir_);
          const fs::path backup_dir = directory / "backups";
          fs::create_directories(backup_dir, fs_error);
          if (fs_error) throw database_error("failed to create database backup directory");
          fs::permissions(backup_dir, fs::perms::owner_all, fs::perm_options::replace);
          const fs::path previous = backup_dir / ("before-import-" + unique_suffix() + ".db");
          TempFile previous_cleanup(previous);
          create_private_database_file(previous);
          SqliteHandle previous_destination(previous);
          backup_database(database_.handle(), previous_destination.get());
          previous_destination.close();
          previous_cleanup.keep();

          try {
            backup_database(staged.handle(), database_.handle());
          } catch (...) {
            SqliteHandle recovery(previous);
            backup_database(recovery.get(), database_.handle());
            throw;
          }
          return nlohmann::json{{"imported", true},
                                {"schema_version", staged_migrations.current_version()}};
        });
      });
}

}  // namespace wt

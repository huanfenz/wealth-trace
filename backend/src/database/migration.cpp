#include "database/migration.hpp"

// 迁移实现：扫描迁移目录、解析版本号、按版本升序在各自事务中执行并记录。

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "common/logging.hpp"
#include "database/database.hpp"
#include "database/statement.hpp"
#include "database/transaction.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

// 扫描到的迁移文件：解析出的版本号、文件名与完整路径。
struct MigrationFile {
  std::int64_t version = 0;
  std::string name;
  std::filesystem::path path;
};

// 从文件名解析版本号：取第一个 '_' 前的前缀，必须非空且为纯数字。
// 例如 "001_init.sql" -> 1；无下划线、前缀非数字或数字溢出时返回 false。
bool parse_version(const std::string& filename, std::int64_t& version) {
  const auto separator = filename.find('_');
  if (separator == std::string::npos || separator == 0) {
    return false;
  }
  const std::string prefix = filename.substr(0, separator);
  for (const char c : prefix) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  try {
    version = std::stoll(prefix);
  } catch (const std::exception&) {
    // 前缀数字过长导致 stoll 溢出，视为非法文件名。
    return false;
  }
  return true;
}

// 以二进制方式整体读取文件（保留原始字节，包括可能的 BOM 与换行）。
std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open migration file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// 扫描目录，收集所有 `*.sql` 且文件名能解析出版本号的迁移，并按版本升序排序。
std::vector<MigrationFile> collect_migrations(const std::string& migrations_dir) {
  namespace fs = std::filesystem;
  const fs::path dir(migrations_dir);
  std::error_code error;
  if (!fs::is_directory(dir, error)) {
    throw std::runtime_error("migrations directory not found: " + migrations_dir);
  }
  std::vector<MigrationFile> migrations;
  for (const auto& entry : fs::directory_iterator(dir, error)) {
    std::error_code entry_error;
    if (!entry.is_regular_file(entry_error)) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    // 只接受 .sql 扩展名。
    if (entry.path().extension() != ".sql") {
      continue;
    }
    MigrationFile migration;
    // 不符合 `<版本号>_<描述>.sql` 命名规则的文件静默跳过。
    if (!parse_version(filename, migration.version)) {
      continue;
    }
    migration.name = filename;
    migration.path = entry.path();
    migrations.push_back(std::move(migration));
  }
  if (error) {
    throw std::runtime_error("failed to read migrations directory: " +
                             error.message());
  }
  // 版本升序执行，保证依赖关系与记录顺序一致。
  std::sort(migrations.begin(), migrations.end(),
            [](const MigrationFile& left, const MigrationFile& right) {
              return left.version < right.version;
            });
  return migrations;
}

}  // namespace

std::int64_t MigrationRunner::current_version() {
  // 记录表可能尚不存在，先确保创建（IF NOT EXISTS），再取最大版本号。
  database_.exec(
      "CREATE TABLE IF NOT EXISTS schema_migration ("
      "  version INTEGER PRIMARY KEY,"
      "  name TEXT NOT NULL,"
      "  applied_at TEXT NOT NULL"
      ");");
  // 空表时 COALESCE 返回 0，表示尚未应用任何迁移。
  Statement statement(database_,
                      "SELECT COALESCE(MAX(version), 0) FROM schema_migration;");
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

std::vector<AppliedMigration> MigrationRunner::applied() {
  // 复用 current_version() 以确保记录表存在。
  current_version();
  Statement statement(database_,
                      "SELECT version, name, applied_at FROM schema_migration "
                      "ORDER BY version ASC;");
  std::vector<AppliedMigration> result;
  while (statement.step()) {
    AppliedMigration record;
    record.version = statement.get_int64(0);
    record.name = statement.get_text(1);
    record.applied_at = statement.get_text(2);
    result.push_back(std::move(record));
  }
  return result;
}

std::vector<std::int64_t> MigrationRunner::run(const std::string& migrations_dir) {
  // 以启动前的最高版本为基线，只执行更高版本的迁移（保证幂等、可重复启动）。
  const std::int64_t start_version = current_version();
  const auto migrations = collect_migrations(migrations_dir);

  std::vector<std::int64_t> applied_versions;
  for (const auto& migration : migrations) {
    if (migration.version <= start_version) {
      continue;
    }
    log_info("applying migration " + migration.name);
    const std::string sql = read_file(migration.path);
    // 每个迁移一个独立事务：SQL 执行与版本记录要么一起成功，要么一起回滚。
    TransactionGuard transaction(database_);
    try {
      database_.exec(sql);
      Statement statement(
          database_,
          "INSERT INTO schema_migration (version, name, applied_at) VALUES (?, ?, ?);");
      // 绑定为 1-based；applied_at 使用 UTC ISO8601 字符串。
      statement.bind(1, migration.version)
          .bind(2, migration.name)
          .bind(3, time_util::now_iso8601())
          .run();
      transaction.commit();
    } catch (const std::exception& error) {
      // 这里不显式 rollback：TransactionGuard 析构时会自动回滚。
      log_error("migration failed: " + migration.name + ": " + error.what());
      throw std::runtime_error("migration " + migration.name + " failed: " + error.what());
    }
    applied_versions.push_back(migration.version);
  }
  return applied_versions;
}

}  // namespace wt

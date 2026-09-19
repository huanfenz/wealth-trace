#include "database/migration.hpp"

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

struct MigrationFile {
  std::int64_t version = 0;
  std::string name;
  std::filesystem::path path;
};

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
    return false;
  }
  return true;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open migration file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

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
    if (entry.path().extension() != ".sql") {
      continue;
    }
    MigrationFile migration;
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
  std::sort(migrations.begin(), migrations.end(),
            [](const MigrationFile& left, const MigrationFile& right) {
              return left.version < right.version;
            });
  return migrations;
}

}  // namespace

std::int64_t MigrationRunner::current_version() {
  database_.exec(
      "CREATE TABLE IF NOT EXISTS schema_migration ("
      "  version INTEGER PRIMARY KEY,"
      "  name TEXT NOT NULL,"
      "  applied_at TEXT NOT NULL"
      ");");
  Statement statement(database_,
                      "SELECT COALESCE(MAX(version), 0) FROM schema_migration;");
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

std::vector<AppliedMigration> MigrationRunner::applied() {
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
  const std::int64_t start_version = current_version();
  const auto migrations = collect_migrations(migrations_dir);

  std::vector<std::int64_t> applied_versions;
  for (const auto& migration : migrations) {
    if (migration.version <= start_version) {
      continue;
    }
    log_info("applying migration " + migration.name);
    const std::string sql = read_file(migration.path);
    TransactionGuard transaction(database_);
    try {
      database_.exec(sql);
      Statement statement(
          database_,
          "INSERT INTO schema_migration (version, name, applied_at) VALUES (?, ?, ?);");
      statement.bind(1, migration.version)
          .bind(2, migration.name)
          .bind(3, time_util::now_iso8601())
          .run();
      transaction.commit();
    } catch (const std::exception& error) {
      log_error("migration failed: " + migration.name + ": " + error.what());
      throw std::runtime_error("migration " + migration.name + " failed: " + error.what());
    }
    applied_versions.push_back(migration.version);
  }
  return applied_versions;
}

}  // namespace wt

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

void preflight_transaction_entries(Database& database) {
  std::vector<std::string> issues;
  {
    Statement s(database,
      "SELECT transfer_group_id,group_concat(id),COUNT(*),"
      "SUM(CASE WHEN type='TRANSFER_OUT' THEN 1 ELSE 0 END),"
      "SUM(CASE WHEN type='TRANSFER_IN' THEN 1 ELSE 0 END),"
      "MIN(amount),MAX(amount),COUNT(DISTINCT household_id) "
      "FROM \"transaction\" WHERE transfer_group_id IS NOT NULL "
      "GROUP BY transfer_group_id HAVING COUNT(*)<>2 OR "
      "SUM(CASE WHEN type='TRANSFER_OUT' THEN 1 ELSE 0 END)<>1 OR "
      "SUM(CASE WHEN type='TRANSFER_IN' THEN 1 ELSE 0 END)<>1 OR "
      "MIN(amount)<>MAX(amount) OR COUNT(DISTINCT household_id)<>1;");
    while(s.step()) issues.push_back("transfer group "+std::to_string(s.get_int64(0))+" rows ["+s.get_text(1)+"]");
  }
  {
    Statement s(database,"SELECT id,type FROM \"transaction\" WHERE "
      "(type IN ('TRANSFER_IN','TRANSFER_OUT') AND transfer_group_id IS NULL) OR "
      "(type IN ('INCOME','EXPENSE','TRANSFER_IN','TRANSFER_OUT','ASSET_PURCHASE') AND amount<=0) OR "
      "(type='ADJUSTMENT' AND amount=0) ORDER BY id;");
    while(s.step())issues.push_back("transaction "+std::to_string(s.get_int64(0))+" has invalid amount or missing transfer group ("+s.get_text(1)+")");
  }
  {
    Statement s(database,"SELECT id,remark FROM \"transaction\" WHERE type='ASSET_PURCHASE';");
    while (s.step()) {
      const auto remark=s.get_optional_text(1);
      const auto marker=remark ? remark->find("(#") : std::string::npos;
      bool valid=remark && marker!=std::string::npos && remark->back()==')' &&
                 marker+3<=remark->size()-1;
      if(valid) {
        for(auto i=marker+2;i+1<remark->size();++i) {
          if((*remark)[i]<'0'||(*remark)[i]>'9') {valid=false;break;}
        }
      }
      if(valid) {
        try {valid=std::stoll(remark->substr(marker+2,remark->size()-marker-3))>0;}
        catch(const std::exception&) {valid=false;}
      }
      if(!valid) issues.push_back("asset purchase transaction "+std::to_string(s.get_int64(0))+" has an ambiguous or malformed target marker");
    }
  }
  {
    const std::string target = "CAST(substr(substr(t.remark,instr(t.remark,'(#')+2),1,instr(substr(t.remark,instr(t.remark,'(#')+2),')')-1) AS INTEGER)";
    Statement s(database,"SELECT t.id FROM \"transaction\" t LEFT JOIN asset a ON a.id="+target+
      " AND a.household_id=t.household_id WHERE t.type='ASSET_PURCHASE' AND "
      "(t.remark IS NULL OR instr(t.remark,'(#')=0 OR a.id IS NULL OR "
      "(SELECT opening_balance FROM asset WHERE id=a.id)<t.amount);");
    while(s.step()) issues.push_back("asset purchase transaction "+std::to_string(s.get_int64(0))+" has no safe target asset");
  }
  {
    const std::string target = "CAST(substr(substr(t.remark,instr(t.remark,'(#')+2),1,instr(substr(t.remark,instr(t.remark,'(#')+2),')')-1) AS INTEGER)";
    Statement s(database,"SELECT group_concat(t.id),a.id FROM \"transaction\" t JOIN asset a ON a.id="+target+
      " WHERE t.type='ASSET_PURCHASE' GROUP BY a.id HAVING SUM(t.amount)>MAX(a.opening_balance);");
    while(s.step()) issues.push_back("asset purchases ["+s.get_text(0)+"] exceed target asset "+std::to_string(s.get_int64(1))+" opening balance");
  }
  {
    Statement s(database,
      "SELECT e.id,e.transfer_group_id FROM recurring_investment_execution e "
      "WHERE e.status IN ('SUCCESS','REVERSED') AND e.transfer_group_id IS NOT NULL AND "
      "(SELECT COUNT(*) FROM \"transaction\" t WHERE t.transfer_group_id=e.transfer_group_id)<>2 "
      "ORDER BY e.id;");
    while(s.step()) issues.push_back("investment execution "+std::to_string(s.get_int64(0))+" references incomplete transfer group "+std::to_string(s.get_int64(1)));
  }
  {
    Statement s(database,
      "SELECT e.id,e.transfer_group_id FROM recurring_investment_execution e "
      "JOIN \"transaction\" o ON o.transfer_group_id=e.transfer_group_id AND o.type='TRANSFER_OUT' "
      "JOIN \"transaction\" i ON i.transfer_group_id=e.transfer_group_id AND i.type='TRANSFER_IN' "
      "WHERE e.status IN ('SUCCESS','REVERSED') AND e.transfer_group_id IS NOT NULL AND "
      "(e.source_asset_id IS NULL OR e.target_asset_id IS NULL OR e.amount IS NULL OR "
      "e.source_asset_id<>o.asset_id OR e.target_asset_id<>i.asset_id OR e.amount<>o.amount OR e.amount<>i.amount) "
      "ORDER BY e.id;");
    while(s.step()) issues.push_back("investment execution "+std::to_string(s.get_int64(0))+" does not match transfer group "+std::to_string(s.get_int64(1)));
  }
  {
    Statement s(database,"SELECT transfer_group_id,group_concat(id) FROM recurring_investment_execution "
      "WHERE status IN ('SUCCESS','REVERSED') AND transfer_group_id IS NOT NULL "
      "GROUP BY transfer_group_id HAVING COUNT(*)>1;");
    while(s.step()) issues.push_back("transfer group "+std::to_string(s.get_int64(0))+" is linked to multiple investment executions ["+s.get_text(1)+"]");
  }
  if(!issues.empty()) {
    std::ostringstream message;
    message << "transaction entry migration preflight failed; repair these records first:";
    for(const auto& issue:issues) message << "\n - " << issue;
    throw std::runtime_error(message.str());
  }
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
    if (migration.version == 13) {
      preflight_transaction_entries(database_);
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

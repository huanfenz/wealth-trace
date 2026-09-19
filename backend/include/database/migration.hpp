#pragma once

// 迁移层：按版本号升序执行 `<版本号>_<描述>.sql` 迁移文件，并把已应用版本记录到 schema_migration 表。

#include <cstdint>
#include <string>
#include <vector>

namespace wt {

class Database;

// 一条已应用迁移的记录，对应 schema_migration 表的一行。
struct AppliedMigration {
  std::int64_t version = 0;
  std::string name;
  std::string applied_at;
};

// 从目录加载并应用带版本的 SQL 迁移。
// 文件名格式：`<版本号>_<描述>.sql`（例如 `001_init.sql`）。
class MigrationRunner {
 public:
  explicit MigrationRunner(Database& database) : database_(database) {}

  // 确保记录表 schema_migration 存在，并返回其中最高的版本号（无记录时返回 0）。
  std::int64_t current_version();

  std::vector<AppliedMigration> applied();

  // 按版本升序应用所有版本号大于当前版本的迁移；每个迁移在各自的事务中执行。
  // 返回本次实际应用的版本号列表；任一迁移失败则抛异常。
  std::vector<std::int64_t> run(const std::string& migrations_dir);

 private:
  Database& database_;
};

}  // namespace wt

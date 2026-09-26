#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <chrono>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "database/statement.hpp"
#include "database/transaction.hpp"
#include "utils/time_util.hpp"

namespace {

using namespace wt;

std::string migrations_dir() {
#ifdef WT_TEST_SOURCE_DIR
  return std::string(WT_TEST_SOURCE_DIR) + "/migrations";
#else
  return "migrations";
#endif
}

class DatabaseTest : public ::testing::Test {
 protected:
  // 每个用例使用独立的内存数据库，并在 SetUp 中执行迁移建好全部表结构。
  void SetUp() override {
    database_.open(":memory:");
    MigrationRunner runner(database_);
    runner.run(migrations_dir());
  }

  Database database_;
};

// 验证迁移执行后版本号与当前迁移集一致，且重复执行幂等（不会重复应用）。
TEST_F(DatabaseTest, MigrationCreatesSchemaAndIsIdempotent) {
  MigrationRunner runner(database_);
  EXPECT_EQ(runner.current_version(), 12);
  EXPECT_EQ(runner.applied().size(), 12u);

  // Running again must not re-apply anything.
  const auto applied_again = runner.run(migrations_dir());
  EXPECT_TRUE(applied_again.empty());
}

// 从旧 schema 11 升级到 12 时，旧的自定义收支分类会建档并关联到原流水。
TEST(DatabaseMigrationTest, ExistingTransactionCategoriesAreBackfilled) {
  namespace fs = std::filesystem;
  const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path old_dir = fs::temp_directory_path() / ("wealth_trace_migrations_" + std::to_string(suffix));
  fs::create_directories(old_dir);
  struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{old_dir};
  const fs::path source(migrations_dir());
  for (const auto& entry : fs::directory_iterator(source)) {
    if (entry.path().extension() == ".sql" && entry.path().filename().string() < "012_")
      fs::copy_file(entry.path(), old_dir / entry.path().filename());
  }

  Database database;
  database.open(":memory:");
  MigrationRunner runner(database);
  runner.run(old_dir.string());
  database.exec("PRAGMA foreign_keys = OFF;");
  const auto now = time_util::now_iso8601();
  Statement household(database, "INSERT INTO household (id, name, created_at, updated_at) VALUES (1, '迁移家庭', ?, ?);");
  household.bind(1, now).bind(2, now).run();
  Statement old_transaction(database,
      "INSERT INTO \"transaction\" (household_id, owner_member_id, asset_id, type, category, amount, "
      "transaction_time, created_at, updated_at) VALUES (1, 1, 1, 'EXPENSE', '宠物医疗', 1234, ?, ?, ?);");
  old_transaction.bind(1, now).bind(2, now).bind(3, now).run();
  database.exec("PRAGMA foreign_keys = ON;");

  const auto applied = runner.run(migrations_dir());
  ASSERT_EQ(applied.size(), 1u);
  EXPECT_EQ(applied.front(), 12);
  Statement migrated(database,
      "SELECT t.category_id, c.name, c.household_id, c.type FROM \"transaction\" t "
      "JOIN transaction_category c ON c.id = t.category_id WHERE t.id = 1;");
  ASSERT_TRUE(migrated.step());
  EXPECT_GT(migrated.get_int64(0), 0);
  EXPECT_EQ(migrated.get_text(1), "宠物医疗");
  EXPECT_EQ(migrated.get_int64(2), 1);
  EXPECT_EQ(migrated.get_text(3), "EXPENSE");
}

// 验证迁移创建了家庭、成员、账户、资产、各明细表以及交易这几张核心表。
TEST_F(DatabaseTest, AllCoreTablesExist) {
  const char* tables[] = {"household",       "household_member",   "account",
                          "asset",           "term_deposit_detail", "stock_fund_detail",
                          "bond_fund_detail", "flexible_term_detail",
                          "commercial_pension_detail", "insurance_detail",
                          "transaction", "transaction_category", "household_category_seed",
                          "system_state"};
  for (const char* table : tables) {
    Statement statement(
        database_,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    statement.bind(1, std::string(table));
    ASSERT_TRUE(statement.step()) << table;
    EXPECT_EQ(statement.get_int64(0), 1) << table;
  }
  Statement removed_bond_table(
      database_,
      "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'bond_detail';");
  ASSERT_TRUE(removed_bond_table.step());
  EXPECT_EQ(removed_bond_table.get_int64(0), 0);

  Statement stock_fund_columns(database_, "PRAGMA table_info(stock_fund_detail);");
  while (stock_fund_columns.step()) {
    EXPECT_NE(stock_fund_columns.get_text(1), "fund_type");
  }
}

// 验证外键约束生效：引用不存在的家庭/成员插入账户会抛 ApiError。
TEST_F(DatabaseTest, ForeignKeysAreEnforced) {
  Statement statement(
      database_,
      "INSERT INTO account (household_id, owner_member_id, name, type, "
      "enabled, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?);");
  ASSERT_THROW(
      {
        statement.bind(1, static_cast<std::int64_t>(999))
            .bind(2, static_cast<std::int64_t>(999))
            .bind(3, "bad")
            .bind(4, "BANK")
            .bind(5, 1)
            .bind(6, time_util::now_iso8601())
            .bind(7, time_util::now_iso8601())
            .run();
      },
      ApiError);
}

// 验证事务守卫：显式 commit 的写入保留，未 commit 的写入在析构时回滚。
TEST_F(DatabaseTest, TransactionGuardCommitsAndRollsBack) {
  const std::string now = time_util::now_iso8601();
  {
    TransactionGuard transaction(database_);
    Statement insert(database_,
                     "INSERT INTO household (name, created_at, updated_at) "
                     "VALUES (?, ?, ?);");
    insert.bind(1, "我的家庭").bind(2, now).bind(3, now).run();
    transaction.commit();
  }
  {
    TransactionGuard transaction(database_);
    Statement insert(database_,
                     "INSERT INTO household (name, created_at, updated_at) "
                     "VALUES (?, ?, ?);");
    insert.bind(1, "回滚家庭").bind(2, now).bind(3, now).run();
    // No commit -> destructor rolls back.
  }

  // 仅提交的那一条存在。
  Statement count(database_, "SELECT COUNT(*) FROM household;");
  ASSERT_TRUE(count.step());
  EXPECT_EQ(count.get_int64(0), 1);
}

// 验证 Prepared Statement 的参数绑定与结果读取：写入后能按条件查出对应行。
TEST_F(DatabaseTest, StatementBindsAndReadsBack) {
  const std::string now = time_util::now_iso8601();
  Statement insert(database_,
                   "INSERT INTO household (name, created_at, updated_at) "
                   "VALUES (?, ?, ?);");
  insert.bind(1, "家庭A").bind(2, now).bind(3, now).run();

  Statement select(database_, "SELECT id, name FROM household WHERE name = ?;");
  select.bind(1, "家庭A");
  ASSERT_TRUE(select.step());
  EXPECT_GT(select.get_int64(0), 0);
  EXPECT_EQ(select.get_text(1), "家庭A");
  EXPECT_FALSE(select.step());  // 结果集只有一行，再次 step 应返回 false
}

}  // namespace

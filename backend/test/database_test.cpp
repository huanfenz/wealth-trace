#include <gtest/gtest.h>

#include <cstdint>
#include <string>

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

// 验证迁移执行后版本号为 1、应用记录 1 条，且重复执行幂等（不会重复应用）。
TEST_F(DatabaseTest, MigrationCreatesSchemaAndIsIdempotent) {
  MigrationRunner runner(database_);
  EXPECT_EQ(runner.current_version(), 1);
  EXPECT_EQ(runner.applied().size(), 1u);

  // Running again must not re-apply anything.
  const auto applied_again = runner.run(migrations_dir());
  EXPECT_TRUE(applied_again.empty());
}

// 验证迁移创建了家庭、成员、账户、资产、各明细表以及交易这几张核心表。
TEST_F(DatabaseTest, AllCoreTablesExist) {
  const char* tables[] = {"household",  "household_member", "account",
                          "asset",      "term_deposit_detail", "fund_detail",
                          "bond_detail", "insurance_detail", "transaction"};
  for (const char* table : tables) {
    Statement statement(
        database_,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    statement.bind(1, std::string(table));
    ASSERT_TRUE(statement.step()) << table;
    EXPECT_EQ(statement.get_int64(0), 1) << table;
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

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
  EXPECT_EQ(runner.current_version(), 13);
  EXPECT_EQ(runner.applied().size(), 13u);

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
  database.exec("INSERT INTO household_member(id,household_id,name,role,status,created_at,updated_at) VALUES(1,1,'成员','OWNER','ACTIVE','2026-01-01 00:00:00','2026-01-01 00:00:00');");
  database.exec("INSERT INTO account(id,household_id,owner_member_id,name,type,enabled,created_at,updated_at) VALUES(1,1,1,'账户','BANK',1,'2026-01-01 00:00:00','2026-01-01 00:00:00');");
  database.exec("INSERT INTO asset(id,household_id,owner_member_id,account_id,name,asset_type,opening_balance,current_balance,status,created_at,updated_at) VALUES(1,1,1,1,'资产','CASH',0,0,'ACTIVE','2026-01-01 00:00:00','2026-01-01 00:00:00');");
  Statement old_transaction(database,
      "INSERT INTO \"transaction\" (household_id, owner_member_id, asset_id, type, category, amount, "
      "transaction_time, created_at, updated_at) VALUES (1, 1, 1, 'EXPENSE', '宠物医疗', 1234, ?, ?, ?);");
  old_transaction.bind(1, now).bind(2, now).bind(3, now).run();
  database.exec("PRAGMA foreign_keys = ON;");

  const auto applied = runner.run(migrations_dir());
  ASSERT_EQ(applied.size(), 2u);
  EXPECT_EQ(applied.front(), 12);
  EXPECT_EQ(applied.back(), 13);
  Statement migrated(database,
      "SELECT t.category_id, c.name, c.household_id, c.type FROM transactions t "
      "JOIN transaction_category c ON c.id = t.category_id WHERE t.id = 1;");
  ASSERT_TRUE(migrated.step());
  EXPECT_GT(migrated.get_int64(0), 0);
  EXPECT_EQ(migrated.get_text(1), "宠物医疗");
  EXPECT_EQ(migrated.get_int64(2), 1);
  EXPECT_EQ(migrated.get_text(3), "EXPENSE");
}

TEST(DatabaseMigrationTest, LegacyRowsBecomeBusinessTransactionsAndEntries) {
  namespace fs=std::filesystem;const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path old_dir=fs::temp_directory_path()/("wealth_trace_tx_migration_"+std::to_string(suffix));fs::create_directories(old_dir);
  struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{old_dir};
  const fs::path source(migrations_dir());for(const auto& e:fs::directory_iterator(source))if(e.path().extension()==".sql"&&e.path().filename().string()<"012_")fs::copy_file(e.path(),old_dir/e.path().filename());
  Database db;db.open(":memory:");MigrationRunner runner(db);runner.run(old_dir.string());db.exec("PRAGMA foreign_keys=OFF;");
  const auto now=time_util::now_iso8601();
  db.exec("INSERT INTO household(id,name,created_at,updated_at) VALUES(1,'H','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO household_member(id,household_id,name,role,status,created_at,updated_at) VALUES(1,1,'M','OWNER','ACTIVE','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO account(id,household_id,owner_member_id,name,type,enabled,created_at,updated_at) VALUES(1,1,1,'A','BANK',1,'2026-01-01','2026-01-01');");
  db.exec("INSERT INTO asset(id,household_id,owner_member_id,account_id,name,asset_type,opening_balance,current_balance,status,created_at,updated_at) VALUES(1,1,1,1,'Cash','CASH',500,500,'ACTIVE','2026-01-01','2026-01-01'),(2,1,1,1,'Fund','STOCK_FUND',100,100,'ACTIVE','2026-01-01','2026-01-01'),(3,1,1,1,'Bought','OTHER',100,100,'ACTIVE','2026-01-01','2026-01-01');");
  auto insert=[&](int id,int asset,const char* type,int amount,const char* remark,int group){Statement s(db,"INSERT INTO \"transaction\"(id,household_id,owner_member_id,asset_id,type,amount,transfer_group_id,balance_before,balance_after,transaction_time,remark,status,created_at,updated_at) VALUES(?,1,1,?,?,?,?,?,?,?,?, 'NORMAL',?,?);");s.bind(1,id).bind(2,asset).bind(3,type).bind(4,amount).bind_optional_int64(5,group?std::optional<std::int64_t>(group):std::nullopt).bind_optional_int64(6,std::nullopt).bind_optional_int64(7,std::nullopt).bind(8,now).bind_optional_text(9,remark?std::optional<std::string>(remark):std::nullopt).bind(10,now).bind(11,now).run();};
  insert(1,1,"INCOME",50,nullptr,0);insert(2,1,"TRANSFER_OUT",20,nullptr,77);insert(3,2,"TRANSFER_IN",20,nullptr,77);insert(4,1,"ADJUSTMENT",-5,nullptr,0);insert(5,1,"ASSET_PURCHASE",20,"购入资产 (#3)",0);insert(6,1,"TRANSFER_OUT",10,nullptr,88);insert(7,2,"TRANSFER_IN",10,nullptr,88);
  db.exec("INSERT INTO recurring_investment_plan(id,household_id,owner_member_id,target_asset_id,source_asset_id,amount,frequency,start_date,next_due_date,status,created_at,updated_at) VALUES(1,1,1,2,1,20,'DAILY','2026-01-01','2026-01-01','ACTIVE','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO recurring_investment_execution(id,plan_id,scheduled_date,amount,source_asset_id,target_asset_id,status,transfer_group_id,created_at,updated_at) VALUES(1,1,'2026-01-01',20,1,2,'SUCCESS',77,'2026-01-01','2026-01-01');");
  db.exec("PRAGMA foreign_keys=ON;");
  db.exec("UPDATE \"transaction\" SET remark='购入资产「基金(#2)」(#3)' WHERE id=5;");
  try{runner.run(migrations_dir());FAIL()<<"expected ambiguous purchase marker to block migration";}
  catch(const std::exception& e){EXPECT_NE(std::string(e.what()).find("asset purchase transaction 5 has an ambiguous"),std::string::npos);}
  EXPECT_EQ(runner.current_version(),12);
  db.exec("UPDATE \"transaction\" SET remark='购入资产 (#3)' WHERE id=5;");
  const auto applied=runner.run(migrations_dir());ASSERT_EQ(applied.size(),1u);
  Statement count(db,"SELECT COUNT(*) FROM transactions;");ASSERT_TRUE(count.step());EXPECT_EQ(count.get_int64(0),5);
  Statement investment(db,"SELECT id,type,action FROM transactions WHERE id=2;");ASSERT_TRUE(investment.step());EXPECT_EQ(investment.get_text(1),"INVESTMENT");EXPECT_EQ(investment.get_text(2),"BUY");
  Statement transfer(db,"SELECT id,type FROM transactions WHERE type='TRANSFER';");ASSERT_TRUE(transfer.step());EXPECT_EQ(transfer.get_int64(0),6);EXPECT_EQ(transfer.get_text(1),"TRANSFER");
  Statement transfer_entries(db,"SELECT COUNT(*) FROM transaction_entries WHERE transaction_id=6;");ASSERT_TRUE(transfer_entries.step());EXPECT_EQ(transfer_entries.get_int64(0),2);
  Statement execution(db,"SELECT transaction_id FROM recurring_investment_execution WHERE id=1;");ASSERT_TRUE(execution.step());EXPECT_EQ(execution.get_int64(0),2);
  Statement adjustment(db,"SELECT direction,amount FROM transaction_entries WHERE transaction_id=4;");ASSERT_TRUE(adjustment.step());EXPECT_EQ(adjustment.get_text(0),"OUT");EXPECT_EQ(adjustment.get_int64(1),5);
  Statement purchase(db,"SELECT opening_balance,current_balance FROM asset WHERE id=3;");ASSERT_TRUE(purchase.step());EXPECT_EQ(purchase.get_int64(0),80);EXPECT_EQ(purchase.get_int64(1),100);
  Statement purchase_entries(db,"SELECT COUNT(*) FROM transaction_entries WHERE transaction_id=5;");ASSERT_TRUE(purchase_entries.step());EXPECT_EQ(purchase_entries.get_int64(0),2);
}

TEST(DatabaseMigrationTest, OrphanRecurringExecutionBlocksEntryMigrationWithRecordId) {
  namespace fs=std::filesystem;const auto suffix=std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path old_dir=fs::temp_directory_path()/("wealth_trace_tx_preflight_"+std::to_string(suffix));fs::create_directories(old_dir);
  struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{old_dir};
  const fs::path source(migrations_dir());for(const auto& e:fs::directory_iterator(source))if(e.path().extension()==".sql"&&e.path().filename().string()<"012_")fs::copy_file(e.path(),old_dir/e.path().filename());
  Database db;db.open(":memory:");MigrationRunner runner(db);runner.run(old_dir.string());db.exec("PRAGMA foreign_keys=OFF;");
  db.exec("INSERT INTO household(id,name,created_at,updated_at) VALUES(1,'H','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO household_member(id,household_id,name,role,status,created_at,updated_at) VALUES(1,1,'M','OWNER','ACTIVE','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO account(id,household_id,owner_member_id,name,type,enabled,created_at,updated_at) VALUES(1,1,1,'A','BANK',1,'2026-01-01','2026-01-01');");
  db.exec("INSERT INTO asset(id,household_id,owner_member_id,account_id,name,asset_type,opening_balance,current_balance,status,created_at,updated_at) VALUES(1,1,1,1,'Cash','CASH',1000,1000,'ACTIVE','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO recurring_investment_plan(id,household_id,owner_member_id,target_asset_id,source_asset_id,amount,frequency,start_date,next_due_date,status,created_at,updated_at) VALUES(1,1,1,1,1,100,'DAILY','2026-01-01','2026-01-01','ACTIVE','2026-01-01','2026-01-01');");
  db.exec("INSERT INTO recurring_investment_execution(id,plan_id,scheduled_date,amount,source_asset_id,target_asset_id,status,transfer_group_id,created_at,updated_at) VALUES(41,1,'2026-01-01',100,1,1,'SUCCESS',987,'2026-01-01','2026-01-01');");
  db.exec("PRAGMA foreign_keys=ON;");
  try{runner.run(migrations_dir());FAIL()<<"expected transaction migration preflight failure";}
  catch(const std::exception& e){EXPECT_NE(std::string(e.what()).find("investment execution 41"),std::string::npos);}
  EXPECT_EQ(runner.current_version(),12);
  Statement legacy(db,"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='transaction';");ASSERT_TRUE(legacy.step());EXPECT_EQ(legacy.get_int64(0),1);
}

// 验证迁移创建了家庭、成员、账户、资产、各明细表以及交易这几张核心表。
TEST_F(DatabaseTest, AllCoreTablesExist) {
  const char* tables[] = {"household",       "household_member",   "account",
                          "asset",           "term_deposit_detail", "stock_fund_detail",
                          "bond_fund_detail", "flexible_term_detail",
                          "commercial_pension_detail", "insurance_detail",
                          "transaction_category", "household_category_seed",
                          "system_state", "transactions", "transaction_entries",
                          "investment_transaction_details"};
  for (const char* table : tables) {
    Statement statement(
        database_,
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    statement.bind(1, std::string(table));
    ASSERT_TRUE(statement.step()) << table;
    EXPECT_EQ(statement.get_int64(0), 1) << table;
  }
  Statement removed_legacy(database_,"SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='transaction';");
  ASSERT_TRUE(removed_legacy.step());EXPECT_EQ(removed_legacy.get_int64(0),0);
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

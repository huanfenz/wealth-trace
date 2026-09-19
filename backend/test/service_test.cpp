#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "model/entities.hpp"
#include "service/account_service.hpp"
#include "service/asset_service.hpp"
#include "service/household_service.hpp"
#include "service/member_service.hpp"
#include "service/statistics_service.hpp"
#include "service/transaction_service.hpp"
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

class ServiceFixture : public ::testing::Test {
 protected:
  // 构造测试数据：内存库跑迁移，建默认家庭「测试家庭」、属主成员「王鹏」和
  // 一个启用的工商银行账户，供各用例在账户下创建资产。
  void SetUp() override {
    database_.open(":memory:");
    MigrationRunner(database_).run(migrations_dir());

    HouseholdService households(database_);
    household_ = households.ensure_default("测试家庭");

    MemberService members(database_);
    member_ = members.create(household_.id, "王鹏", MemberRole::Owner, MemberStatus::Active);

    AccountService accounts(database_);
    account_ = accounts.create(household_.id, member_.id, "工商银行", AccountType::Bank,
                               std::nullopt, std::nullopt, std::nullopt, true);
  }

  // 在夹具账户下按给定名称、类型和期初余额创建资产，返回创建结果。
  Asset make_asset(const std::string& name, AssetType type, std::int64_t opening_balance) {
    AssetService assets(database_);
    AssetCreateInput input;
    input.account_id = account_.id;
    input.name = name;
    input.asset_type = type;
    input.opening_balance = opening_balance;
    return assets.create(input).asset;
  }

  Database database_;
  Household household_;
  HouseholdMember member_;
  Account account_;
};

// 验证收入使余额增加、支出使余额减少，且流水记录的前后余额与资产最终余额一致。
TEST_F(ServiceFixture, IncomeAndExpenseUpdateBalance) {
  const Asset cash = make_asset("活期", AssetType::Cash, 2000000);
  TransactionService transactions(database_);

  const auto income = transactions.record_income(cash.id, "工资", 1000000, "", std::nullopt);
  EXPECT_EQ(income.balance_before.value(), 2000000);
  EXPECT_EQ(income.balance_after.value(), 3000000);

  const auto expense = transactions.record_expense(cash.id, "餐饮", 3500, "", std::nullopt);
  EXPECT_EQ(expense.balance_before.value(), 3000000);
  EXPECT_EQ(expense.balance_after.value(), 2996500);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 2996500);
}

// 验证转账产生 TRANSFER_OUT + TRANSFER_IN 两条流水、共用同一 transfer_group_id，
// 且转出/转入资产余额分别减少和增加。
TEST_F(ServiceFixture, TransferMovesFundsAndLinksGroup) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  const Asset fund = make_asset("某基金", AssetType::Fund, 500000);
  TransactionService transactions(database_);

  const auto result = transactions.transfer(cash.id, fund.id, 300000, "", std::nullopt);
  EXPECT_EQ(result.outgoing.type, TransactionType::TransferOut);
  EXPECT_EQ(result.incoming.type, TransactionType::TransferIn);
  ASSERT_TRUE(result.outgoing.transfer_group_id.has_value());
  ASSERT_TRUE(result.incoming.transfer_group_id.has_value());
  EXPECT_EQ(result.outgoing.transfer_group_id, result.incoming.transfer_group_id);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 700000);
  EXPECT_EQ(assets.get(fund.id).current_balance, 800000);

  TransactionQuery query;
  query.household_id = household_.id;
  EXPECT_EQ(transactions.count(query), 2);
}

// 验证禁止跨家庭转账，失败后两个资产的余额均保持不变。
TEST_F(ServiceFixture, CrossHouseholdTransferRejected) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);

  HouseholdService households(database_);
  const Household other = households.create("另一个家庭");
  MemberService members(database_);
  const HouseholdMember other_member =
      members.create(other.id, "配偶", MemberRole::Member, MemberStatus::Active);
  AccountService accounts(database_);
  const Account other_account =
      accounts.create(other.id, other_member.id, "招商银行", AccountType::Bank, std::nullopt,
                      std::nullopt, std::nullopt, true);
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = other_account.id;
  input.name = "活期";
  input.asset_type = AssetType::Cash;
  input.opening_balance = 500000;
  const Asset other_cash = assets.create(input).asset;

  TransactionService transactions(database_);
  EXPECT_THROW(transactions.transfer(cash.id, other_cash.id, 100000, "", std::nullopt),
               ApiError);

  EXPECT_EQ(assets.get(cash.id).current_balance, 1000000);
  EXPECT_EQ(assets.get(other_cash.id).current_balance, 500000);
}

// 验证禁止向同一资产自身转账。
TEST_F(ServiceFixture, TransferToSameAssetRejected) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.transfer(cash.id, cash.id, 100000, "", std::nullopt), ApiError);
}

// 验证调整金额可正可负：正数入账、负数扣减，累计后反映到当前余额。
TEST_F(ServiceFixture, AdjustmentAppliesSignedAmount) {
  const Asset fund = make_asset("某基金", AssetType::Fund, 5000000);
  TransactionService transactions(database_);

  transactions.record_adjustment(fund.id, 100000, "", std::nullopt);
  transactions.record_adjustment(fund.id, -250000, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(fund.id).current_balance, 4850000);
}

// 验证已关闭（Closed）的资产禁止新增交易。
TEST_F(ServiceFixture, ClosedAssetRejectsTransactions) {
  Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  AssetService assets(database_);
  assets.update_status(cash.id, AssetStatus::Closed);

  TransactionService transactions(database_);
  EXPECT_THROW(transactions.record_income(cash.id, "工资", 100, "", std::nullopt), ApiError);
}

// 验证负债类资产的消费支出会使其负数余额变得更负（负债增加）。
TEST_F(ServiceFixture, LiabilityExpenseIncreasesDebt) {
  const Asset card = make_asset("信用卡", AssetType::Liability, 0);
  TransactionService transactions(database_);
  transactions.record_expense(card.id, "餐饮", 10000, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(card.id).current_balance, -10000);
}

// 验证统计口径：总资产排除负债、总负债取绝对值、净资产为全部 active 余额之和，
// 月度收入/支出仅统计当月 INCOME/EXPENSE。
TEST_F(ServiceFixture, StatisticsReflectBalancesAndFlow) {
  const Asset cash = make_asset("活期", AssetType::Cash, 2000000);
  const Asset card = make_asset("信用卡", AssetType::Liability, 0);
  TransactionService transactions(database_);
  transactions.record_income(cash.id, "工资", 1000000, "", std::nullopt);
  transactions.record_expense(cash.id, "餐饮", 50000, "", std::nullopt);
  transactions.record_expense(card.id, "购物", 30000, "", std::nullopt);

  StatisticsService statistics(database_);
  const std::string today = time_util::today_iso8601();
  const int year = std::stoi(today.substr(0, 4));
  const int month = std::stoi(today.substr(5, 2));

  const auto overview = statistics.overview(household_.id, year, month);
  EXPECT_EQ(overview.total_assets, 2950000);
  EXPECT_EQ(overview.total_liabilities, 30000);
  EXPECT_EQ(overview.net_worth, 2920000);
  EXPECT_EQ(overview.month_income, 1000000);
  EXPECT_EQ(overview.month_expense, 80000);
  EXPECT_EQ(overview.month_balance, 920000);
}

// 验证账户下已有资产时，禁止修改账户属主。
TEST_F(ServiceFixture, AccountOwnerChangeBlockedWhileAssetsExist) {
  make_asset("活期", AssetType::Cash, 1000000);
  MemberService members(database_);
  const HouseholdMember second =
      members.create(household_.id, "配偶", MemberRole::Member, MemberStatus::Active);

  AccountService accounts(database_);
  EXPECT_THROW(accounts.update(account_.id, second.id, account_.name, account_.type,
                               std::nullopt, std::nullopt, std::nullopt, true),
               ApiError);
}

// 验证资产已有交易后，禁止再修改 opening_balance。
TEST_F(ServiceFixture, OpeningBalanceChangeBlockedAfterTransactions) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  transactions.record_income(cash.id, "工资", 100, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_THROW(assets.update_metadata(cash.id, cash.name, 2000000, std::nullopt), ApiError);
}

// 验证无交易时允许修改 opening_balance，且当前余额同步调整为新期初值。
TEST_F(ServiceFixture, OpeningBalanceChangeWithoutTransactionsAdjustsBalance) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  AssetService assets(database_);
  const Asset updated = assets.update_metadata(cash.id, cash.name, 1500000, std::nullopt);
  EXPECT_EQ(updated.opening_balance, 1500000);
  EXPECT_EQ(updated.current_balance, 1500000);
}

// 验证明细类型必须与资产类型一致：现金资产携带基金明细应被拒绝。
TEST_F(ServiceFixture, DetailTypeMustMatchAssetType) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "活期";
  input.asset_type = AssetType::Cash;
  input.opening_balance = 0;
  FundDetail fund;
  fund.fund_code = "000001";
  input.fund = fund;
  EXPECT_THROW(assets.create(input), ApiError);
}

// 验证非法金额被拒绝：收入不得为 0、支出不得为负、转账金额不得为 0。
TEST_F(ServiceFixture, InvalidAmountsRejected) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.record_income(cash.id, std::nullopt, 0, "", std::nullopt),
               ApiError);
  EXPECT_THROW(transactions.record_expense(cash.id, std::nullopt, -100, "", std::nullopt),
               ApiError);
  EXPECT_THROW(transactions.transfer(cash.id, cash.id, 0, "", std::nullopt), ApiError);
}

// 验证负债资产的期初余额必须为非正数，传入正数应被拒绝。
TEST_F(ServiceFixture, LiabilityOpeningBalanceMustBeNonPositive) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "信用卡";
  input.asset_type = AssetType::Liability;
  input.opening_balance = 100;
  EXPECT_THROW(assets.create(input), ApiError);
}

}  // namespace

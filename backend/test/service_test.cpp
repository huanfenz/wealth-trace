#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "database/statement.hpp"
#include "model/entities.hpp"
#include "repository/system_state_repository.hpp"
#include "repository/transaction_repository.hpp"
#include "service/account_service.hpp"
#include "service/category_service.hpp"
#include "service/asset_service.hpp"
#include "service/daily_maintenance_service.hpp"
#include "service/household_service.hpp"
#include "service/member_service.hpp"
#include "service/statistics_service.hpp"
#include "service/transaction_service.hpp"
#include "utils/term_date.hpp"
#include "utils/time_util.hpp"
#include "utils/flexible_term.hpp"

namespace {

using namespace wt;

std::string migrations_dir() {
#ifdef WT_TEST_SOURCE_DIR
  return std::string(WT_TEST_SOURCE_DIR) + "/migrations";
#else
  return "migrations";
#endif
}

// 业务时区下「今天 12:00」对应的 UTC 时间戳，确保流水稳定落在业务当前月，
// 不受运行时刻 UTC/业务日期跨日影响。
std::string business_noon_utc() {
  return time_util::business_to_utc(time_util::business_today() + " 12:00:00");
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
    return assets.create(household_.id, input).asset;
  }

  Database database_;
  Household household_;
  HouseholdMember member_;
  Account account_;
};

TEST_F(ServiceFixture, FlexibleTermDetailAndTransferWindow) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "定活理财";
  input.asset_type = AssetType::FlexibleTerm;
  input.opening_balance = 10000;
  const auto today = time_util::business_today();
  input.flexible_term = FlexibleTermDetail{0, time_util::add_days(today, -31), 180};
  const auto created = assets.create(household_.id, input);
  ASSERT_TRUE(created.flexible_term.has_value());
  ASSERT_TRUE(assets.get_bundle(created.asset.id).flexible_term.has_value());
  const auto target = make_asset("活期", AssetType::Cash, 0);
  TransactionService transactions(database_);
  input.flexible_term->purchase_date = today;
  input.name = "未到赎回日的定活理财";
  const auto locked = assets.create(household_.id, input);
  const auto fund = make_asset("投资目标", AssetType::StockFund, 0);
  EXPECT_THROW(transactions.investment_buy(household_.id, locked.asset.id, fund.id,
                                          100, business_noon_utc(), std::nullopt), ApiError);
  EXPECT_EQ(assets.get(locked.asset.id).current_balance, 10000);
  input.flexible_term->purchase_date = time_util::add_days(today, -31);
  if (flexible_term::can_transfer(*input.flexible_term, today)) {
    EXPECT_NO_THROW(transactions.transfer(household_.id, created.asset.id, target.id,
                                           100, business_noon_utc(), std::nullopt));
  } else {
    EXPECT_THROW(transactions.transfer(household_.id, created.asset.id, target.id,
                                        100, business_noon_utc(), std::nullopt), ApiError);
  }
  input.flexible_term->purchase_date = time_util::add_days(today, -181);
  input.name = "已满期定活理财";
  const auto matured = assets.create(household_.id, input);
  EXPECT_NO_THROW(transactions.transfer(household_.id, matured.asset.id, target.id,
                                         100, business_noon_utc(), std::nullopt));
  input.flexible_term->holding_period_days = 90;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);
}

TEST_F(ServiceFixture, CommercialPensionReservationWindowDoesNotRestrictAction) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "商业养老金";
  input.asset_type = AssetType::CommercialPension;
  input.opening_balance = 10000;
  const auto now = time_util::utc_to_business(time_util::now_iso8601());
  CommercialPensionDetail detail;
  detail.purchase_time = time_util::add_days(now.substr(0, 10), -10) + now.substr(10);
  detail.holding_period_value = 1;
  detail.holding_period_unit = TermUnit::Year;
  detail.reservation_window_start = time_util::add_days(now.substr(0, 10), 1) + now.substr(10);
  detail.reservation_window_end = time_util::add_days(now.substr(0, 10), 2) + now.substr(10);
  input.commercial_pension = detail;
  const auto created = assets.create(household_.id, input);
  ASSERT_TRUE(created.commercial_pension.has_value());
  EXPECT_FALSE(created.commercial_pension->redeem_at_maturity);
  const auto target = make_asset("活期", AssetType::Cash, 0);
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.transfer(household_.id, created.asset.id, target.id,
      100, business_noon_utc(), std::nullopt), ApiError);
  detail.redeem_at_maturity = true;
  // 预约提醒尚未开始，但仍允许选择到期赎回。
  const auto updated = assets.update_detail(created.asset.id, AssetType::CommercialPension,
      nullptr, nullptr, nullptr, nullptr, &detail, nullptr);
  ASSERT_TRUE(updated.commercial_pension.has_value());
  EXPECT_TRUE(updated.commercial_pension->redeem_at_maturity);
  EXPECT_TRUE(updated.commercial_pension->redeem_at.has_value());
  EXPECT_EQ(assets.get_bundle(created.asset.id).commercial_pension->redeem_at,
            updated.commercial_pension->redeem_at);
  EXPECT_THROW(transactions.transfer(household_.id, created.asset.id, target.id,
      100, business_noon_utc(), std::nullopt), ApiError);
  detail.reservation_window_start = std::nullopt;
  detail.reservation_window_end = std::nullopt;
  detail.redeem_at_maturity = false;
  const auto renewed = assets.update_detail(created.asset.id, AssetType::CommercialPension,
      nullptr, nullptr, nullptr, nullptr, &detail, nullptr);
  EXPECT_FALSE(renewed.commercial_pension->redeem_at_maturity);
  input.commercial_pension = detail;
  input.commercial_pension->redeem_at_maturity = true;
  EXPECT_TRUE(assets.create(household_.id, input).commercial_pension->redeem_at_maturity);
}

// 验证收入使余额增加、支出使余额减少，且流水记录的前后余额与资产最终余额一致。
TEST_F(ServiceFixture, IncomeAndExpenseUpdateBalance) {
  const Asset cash = make_asset("活期", AssetType::Cash, 2000000);
  TransactionService transactions(database_);

  const auto income = transactions.record_income(household_.id, cash.id, "工资", 1000000, "", std::nullopt);
  auto income_entries=TransactionRepository(database_).entries(income.id);
  ASSERT_EQ(income_entries.size(),1u);
  EXPECT_EQ(income_entries[0].balance_before.value(),2000000);
  EXPECT_EQ(income_entries[0].balance_after.value(),3000000);

  const auto expense = transactions.record_expense(household_.id, cash.id, "餐饮", 3500, "", std::nullopt);
  auto expense_entries=TransactionRepository(database_).entries(expense.id);
  ASSERT_EQ(expense_entries.size(),1u);
  EXPECT_EQ(expense_entries[0].balance_before.value(),3000000);
  EXPECT_EQ(expense_entries[0].balance_after.value(),2996500);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 2996500);
}

// 分类归家庭和收支类型管理；改名同步到历史流水及统计，停用后禁止新记账。
TEST_F(ServiceFixture, CategoriesAreManagedAndLinkedToTransactions) {
  CategoryConfig defaults;
  defaults.income = {"工资"};
  defaults.expense = {"餐饮"};
  CategoryService categories(database_, defaults);
  const auto incomes = categories.list(household_.id, TransactionType::Income);
  ASSERT_EQ(incomes.size(), 1u);
  EXPECT_EQ(incomes.front().name, "工资");
  const auto expenses = categories.list(household_.id, TransactionType::Expense);
  ASSERT_EQ(expenses.size(), 1u);

  const auto created = categories.create(household_.id, TransactionType::Expense, "宠物医疗");
  EXPECT_THROW(categories.create(household_.id, TransactionType::Expense, "宠物医疗"), ApiError);

  const Asset cash = make_asset("活期", AssetType::Cash, 100000);
  TransactionService transactions(database_);
  const std::string at = business_noon_utc();
  const auto recorded = transactions.record_expense(household_.id, cash.id, created.id,
      1234, at, std::nullopt);
  EXPECT_EQ(recorded.category_id, created.id);
  EXPECT_EQ(recorded.category, "宠物医疗");

  const auto renamed = categories.update(household_.id, created.id, "宠物健康", 2);
  EXPECT_EQ(renamed.name, "宠物健康");
  EXPECT_EQ(transactions.get(recorded.id).category, "宠物健康");
  StatisticsService statistics(database_);
  const auto business_at = time_util::utc_to_business(at);
  const auto period = statistics.period(household_.id, business_at, business_at, std::nullopt);
  ASSERT_EQ(period.expense_categories.size(), 1u);
  EXPECT_EQ(period.expense_categories.front().category, "宠物健康");
  EXPECT_EQ(period.expense_categories.front().amount, 1234);

  categories.set_active(household_.id, created.id, false);
  EXPECT_THROW(transactions.record_expense(household_.id, cash.id, created.id,
      100, at, std::nullopt), ApiError);
  EXPECT_TRUE(categories.set_active(household_.id, created.id, true).active);
  EXPECT_THROW(categories.update(household_.id + 999, created.id, "越权改名", 0), ApiError);
}

// 转账以一个业务交易保存，Entry 分别描述转出与转入资产变化。
TEST_F(ServiceFixture, TransferIsOneTransactionWithTwoEntries) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  const Asset stock_fund = make_asset("某股票基金", AssetType::StockFund, 500000);
  TransactionService transactions(database_);

  const auto result = transactions.transfer(household_.id, cash.id, stock_fund.id, 300000, "", std::nullopt);
  EXPECT_EQ(result.type, TransactionType::Transfer);
  const auto entries=TransactionRepository(database_).entries(result.id);
  ASSERT_EQ(entries.size(),2u);
  EXPECT_EQ(entries[0].amount,300000);
  EXPECT_EQ(entries[1].amount,300000);
  EXPECT_NE(entries[0].direction,entries[1].direction);
  const auto dto=transactions.dto(result.id);
  EXPECT_EQ(dto.direction,DisplayDirection::Neutral);
  EXPECT_EQ(dto.amount,300000);
  EXPECT_EQ(dto.subtitle,"活期 → 某股票基金");

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 700000);
  EXPECT_EQ(assets.get(stock_fund.id).current_balance, 800000);

  TransactionQuery query;
  query.household_id = household_.id;
  EXPECT_EQ(transactions.count(query), 1);
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
  const Asset other_cash = assets.create(other.id, input).asset;

  TransactionService transactions(database_);
  EXPECT_THROW(transactions.transfer(household_.id, cash.id, other_cash.id, 100000, "", std::nullopt),
               ApiError);

  EXPECT_EQ(assets.get(cash.id).current_balance, 1000000);
  EXPECT_EQ(assets.get(other_cash.id).current_balance, 500000);
}

// 写接口中的 household_id 是实际的归属边界：不得借用其他家庭的账户或资产。
TEST_F(ServiceFixture, HouseholdScopedWritesRejectForeignResources) {
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
  input.name = "外部活期";
  input.asset_type = AssetType::Cash;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);

  const Asset foreign_asset = assets.create(other.id, input).asset;
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.record_income(household_.id, foreign_asset.id, "工资", 100,
                                          "", std::nullopt),
               ApiError);
}

// 多个请求共享同一连接时，服务层锁必须让整笔记账串行，不能让事务交叉。
TEST_F(ServiceFixture, ConcurrentRecordsRemainConsistent) {
  const Asset cash = make_asset("活期", AssetType::Cash, 0);
  constexpr int kWriters = 8;
  std::barrier start(kWriters);
  std::atomic<int> successes = 0;
  std::vector<std::thread> threads;
  threads.reserve(kWriters);
  for (int i = 0; i < kWriters; ++i) {
    threads.emplace_back([&] {
      TransactionService transactions(database_);
      start.arrive_and_wait();
      transactions.record_income(household_.id, cash.id, "工资", 100, "", std::nullopt);
      ++successes;
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(successes, kWriters);
  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, kWriters * 100);
  TransactionService transactions(database_);
  TransactionQuery query;
  query.household_id = household_.id;
  EXPECT_EQ(transactions.count(query), kWriters);
}

// 验证禁止向同一资产自身转账。
TEST_F(ServiceFixture, TransferToSameAssetRejected) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.transfer(household_.id, cash.id, cash.id, 100000, "", std::nullopt), ApiError);
}

// 验证调整金额可正可负：正数入账、负数扣减，累计后反映到当前余额。
TEST_F(ServiceFixture, AdjustmentAppliesSignedAmount) {
  const Asset stock_fund = make_asset("某股票基金", AssetType::StockFund, 5000000);
  TransactionService transactions(database_);

  transactions.record_adjustment(household_.id, stock_fund.id, 100000, "", std::nullopt);
  transactions.record_adjustment(household_.id, stock_fund.id, -250000, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(stock_fund.id).current_balance, 4850000);
}

// 验证已关闭（Closed）的资产禁止新增交易。
TEST_F(ServiceFixture, ClosedAssetRejectsTransactions) {
  Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  AssetService assets(database_);
  assets.update_status(cash.id, AssetStatus::Closed);

  TransactionService transactions(database_);
  EXPECT_THROW(transactions.record_income(household_.id, cash.id, "工资", 100, "", std::nullopt), ApiError);
}

// 验证负债类资产的消费支出会使其负数余额变得更负（负债增加）。
TEST_F(ServiceFixture, LiabilityExpenseIncreasesDebt) {
  const Asset card = make_asset("信用卡", AssetType::Liability, 0);
  TransactionService transactions(database_);
  transactions.record_expense(household_.id, card.id, "餐饮", 10000, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(card.id).current_balance, -10000);
}

// 验证统计口径：总资产排除负债、总负债取绝对值、净资产为全部 active 余额之和，
// 月度收入/支出仅统计当月 INCOME/EXPENSE。
TEST_F(ServiceFixture, StatisticsReflectBalancesAndFlow) {
  const Asset cash = make_asset("活期", AssetType::Cash, 2000000);
  const Asset card = make_asset("信用卡", AssetType::Liability, 0);
  TransactionService transactions(database_);
  const std::string at = business_noon_utc();
  transactions.record_income(household_.id, cash.id, "工资", 1000000, at, std::nullopt);
  transactions.record_expense(household_.id, cash.id, "餐饮", 50000, at, std::nullopt);
  transactions.record_expense(household_.id, card.id, "购物", 30000, at, std::nullopt);

  StatisticsService statistics(database_);
  const std::string today = time_util::business_today();
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

// 验证账户下仍有资产时，禁止删除账户。
TEST_F(ServiceFixture, AccountDeleteBlockedWhileAssetsExist) {
  make_asset("活期", AssetType::Cash, 1000000);

  AccountService accounts(database_);
  EXPECT_THROW(accounts.remove(account_.id), ApiError);
}

// 验证无资产账户可正常删除，删除后再查询抛 not_found。
TEST_F(ServiceFixture, AccountDeleteRemovesEmptyAccount) {
  AccountService accounts(database_);
  const Account empty = accounts.create(household_.id, member_.id, "现金", AccountType::Cash,
                                        std::nullopt, std::nullopt, std::nullopt, true);
  accounts.remove(empty.id);
  EXPECT_THROW(accounts.get(empty.id), ApiError);
}

// 验证现金/支付宝/微信账户在未提供机构名时自动填入类型中文名，其它类型保持为空。
TEST_F(ServiceFixture, CashLikeAccountGetsDefaultInstitution) {
  AccountService accounts(database_);
  const Account cash = accounts.create(household_.id, member_.id, "零钱", AccountType::Cash,
                                       std::nullopt, std::nullopt, std::nullopt, true);
  EXPECT_EQ(cash.institution_name.value_or(""), "现金");

  const Account alipay =
      accounts.create(household_.id, member_.id, "支付宝", AccountType::Alipay, std::nullopt,
                      std::nullopt, std::nullopt, true);
  EXPECT_EQ(alipay.institution_name.value_or(""), "支付宝");

  const Account wechat =
      accounts.create(household_.id, member_.id, "微信", AccountType::Wechat, std::nullopt,
                      std::nullopt, std::nullopt, true);
  EXPECT_EQ(wechat.institution_name.value_or(""), "微信");

  const Account bank = accounts.create(household_.id, member_.id, "某银行", AccountType::Bank,
                                       std::nullopt, std::nullopt, std::nullopt, true);
  EXPECT_FALSE(bank.institution_name.has_value());
}

// 验证资产已有交易后，禁止再修改 opening_balance。
TEST_F(ServiceFixture, OpeningBalanceChangeBlockedAfterTransactions) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  transactions.record_income(household_.id, cash.id, "工资", 100, "", std::nullopt);

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

// 验证明细类型必须与资产类型一致：现金资产携带股票基金明细应被拒绝。
TEST_F(ServiceFixture, DetailTypeMustMatchAssetType) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "活期";
  input.asset_type = AssetType::Cash;
  input.opening_balance = 0;
  StockFundDetail stock_fund;
  stock_fund.fund_code = "000001";
  input.stock_fund = stock_fund;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);
}

// 验证非法金额被拒绝：收入不得为 0、支出不得为负、转账金额不得为 0。
TEST_F(ServiceFixture, InvalidAmountsRejected) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.record_income(household_.id, cash.id, std::nullopt, 0, "", std::nullopt),
               ApiError);
  EXPECT_THROW(transactions.record_expense(household_.id, cash.id, std::nullopt, -100, "", std::nullopt),
               ApiError);
  EXPECT_THROW(transactions.transfer(household_.id, cash.id, cash.id, 0, "", std::nullopt), ApiError);
}

// 验证负债资产的期初余额必须为非正数，传入正数应被拒绝。
TEST_F(ServiceFixture, LiabilityOpeningBalanceMustBeNonPositive) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "信用卡";
  input.asset_type = AssetType::Liability;
  input.opening_balance = 100;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);
}

// 有交易的资产禁止硬删除，交易与资产历史保持完整。
TEST_F(ServiceFixture, AssetWithHistoryCannotBeDeleted) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  transactions.record_income(household_.id, cash.id, "工资", 100000, "", std::nullopt);
  transactions.record_expense(household_.id, cash.id, "餐饮", 5000, "", std::nullopt);

  AssetService assets(database_);
  EXPECT_THROW(assets.remove(cash.id), ApiError);
  EXPECT_NO_THROW(assets.get(cash.id));

  TransactionQuery query;
  query.household_id = household_.id;
  query.asset_id = cash.id;
  EXPECT_EQ(transactions.count(query), 2);
}

// 验证删除收入流水会回滚资产余额。
TEST_F(ServiceFixture, DeleteTransactionRollsBackBalance) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  TransactionService transactions(database_);
  const auto income =
      transactions.record_income(household_.id, cash.id, "工资", 1000000, "", std::nullopt);
  EXPECT_EQ(transactions.remove(income.id), 1);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 1000000);
}

// 验证删除转账流水会成对删除两条，并回滚两端资产余额。
TEST_F(ServiceFixture, DeleteTransferRemovesPairAndRestoresBalances) {
  const Asset cash = make_asset("活期", AssetType::Cash, 1000000);
  const Asset stock_fund = make_asset("某股票基金", AssetType::StockFund, 500000);
  TransactionService transactions(database_);
  const auto result =
      transactions.transfer(household_.id, cash.id, stock_fund.id, 300000, "", std::nullopt);
  EXPECT_EQ(transactions.remove(result.id), 1);

  AssetService assets(database_);
  EXPECT_EQ(assets.get(cash.id).current_balance, 1000000);
  EXPECT_EQ(assets.get(stock_fund.id).current_balance, 500000);

  TransactionQuery query;
  query.household_id = household_.id;
  EXPECT_EQ(transactions.count(query), 0);
}

TEST_F(ServiceFixture, DeleteTransferWithoutRollbackKeepsBothBalances) {
  const Asset source = make_asset("活期", AssetType::Cash, 10000);
  const Asset target = make_asset("储蓄", AssetType::Cash, 1000);
  TransactionService transactions(database_);
  const auto transfer = transactions.transfer(household_.id, source.id, target.id, 500, "",
                                              std::nullopt);
  EXPECT_EQ(transactions.remove(transfer.id, false), 1);
  AssetService assets(database_);
  EXPECT_EQ(assets.get(source.id).current_balance, 9500);
  EXPECT_EQ(assets.get(target.id).current_balance, 1500);
  TransactionQuery query;
  query.household_id = household_.id;
  EXPECT_EQ(transactions.count(query), 0);
}

TEST_F(ServiceFixture, InvestmentBuyIsNeutralAndStoresBothAssetMovements) {
  const Asset source=make_asset("银行卡",AssetType::Cash,500000);
  const Asset target=make_asset("债基",AssetType::BondFund,0);
  TransactionService service(database_);
  const auto tx=service.investment_buy(household_.id,source.id,target.id,125000,"",std::nullopt);
  EXPECT_EQ(tx.type,TransactionType::Investment);
  EXPECT_EQ(tx.action,InvestmentAction::Buy);
  const auto entries=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(entries.size(),2u);
  EXPECT_EQ(entries[0].direction,TransactionDirection::Out);
  EXPECT_EQ(entries[1].direction,TransactionDirection::In);
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,375000);
  EXPECT_EQ(AssetService(database_).get(target.id).current_balance,125000);
  const auto dto=service.dto(tx.id);
  EXPECT_EQ(dto.direction,DisplayDirection::Neutral);
  EXPECT_EQ(dto.title,"买入");
  EXPECT_EQ(dto.subtitle,"银行卡 → 债基");
}

TEST_F(ServiceFixture, LinkedRecurringInvestmentCannotBeEditedIntoAnotherBusinessType) {
  const Asset source=make_asset("付款资产",AssetType::Cash,500000);
  const Asset target=make_asset("定投基金",AssetType::StockFund,0);
  TransactionService service(database_);
  const auto tx=service.investment_buy(household_.id,source.id,target.id,1000,"",std::nullopt);
  const auto now=time_util::now_iso8601();
  Statement plan(database_,"INSERT INTO recurring_investment_plan "
    "(household_id,owner_member_id,target_asset_id,source_asset_id,amount,frequency,start_date,next_due_date,status,created_at,updated_at) "
    "VALUES (?,?,?,?,?,'DAILY','2026-09-26','2026-09-27','ACTIVE',?,?);");
  plan.bind(1,household_.id).bind(2,source.owner_member_id).bind(3,target.id).bind(4,source.id)
      .bind(5,1000).bind(6,now).bind(7,now).run();
  const auto plan_id=database_.last_insert_rowid();
  Statement execution(database_,"INSERT INTO recurring_investment_execution "
    "(plan_id,scheduled_date,amount,source_asset_id,target_asset_id,status,transaction_id,created_at,updated_at) "
    "VALUES (?,?,?,?,?,'SUCCESS',?,?,?);");
  execution.bind(1,plan_id).bind(2,time_util::business_today()).bind(3,1000).bind(4,source.id)
      .bind(5,target.id).bind(6,tx.id).bind(7,now).bind(8,now).run();

  TransactionInput edited;edited.type=TransactionType::Transfer;edited.transaction_time=tx.transaction_time;
  edited.entries={{source.id,TransactionDirection::Out,1000},{target.id,TransactionDirection::In,1000}};
  EXPECT_THROW(service.update(tx.id,edited),ApiError);
  EXPECT_EQ(service.get(tx.id).type,TransactionType::Investment);
  EXPECT_FALSE(service.dto(tx.id).editable);
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,499000);
  Statement check(database_,"SELECT status,transaction_id FROM recurring_investment_execution WHERE plan_id=?;");
  check.bind(1,plan_id);ASSERT_TRUE(check.step());EXPECT_EQ(check.get_text(0),"SUCCESS");EXPECT_EQ(check.get_int64(1),tx.id);
}

TEST_F(ServiceFixture, UpdatingCategoryPreservesHistoricalEntrySnapshots) {
  const Asset cash=make_asset("银行卡",AssetType::Cash,1000);
  TransactionService service(database_);
  const auto income=service.record_income(household_.id,cash.id,"工资",100,"",std::nullopt);
  service.record_expense(household_.id,cash.id,std::nullopt,50,"",std::nullopt);
  const auto before=TransactionRepository(database_).entries(income.id);
  ASSERT_EQ(before.size(),1u);
  AssetService(database_).update_status(cash.id,AssetStatus::Closed);
  EXPECT_NO_THROW(service.update_category(income.id,std::nullopt));
  const auto after=TransactionRepository(database_).entries(income.id);
  ASSERT_EQ(after.size(),1u);
  EXPECT_EQ(after[0].id,before[0].id);
  EXPECT_EQ(after[0].balance_before,before[0].balance_before);
  EXPECT_EQ(after[0].balance_after,before[0].balance_after);
  EXPECT_EQ(AssetService(database_).get(cash.id).current_balance,1050);
}

TEST_F(ServiceFixture, AssetTransactionViewUsesTheSelectedEntriesDirection) {
  const Asset source=make_asset("银行卡",AssetType::Cash,50000);
  const Asset target=make_asset("支付宝",AssetType::Cash,5000);
  TransactionService service(database_);
  const auto tx=service.transfer(household_.id,source.id,target.id,1200,"",std::nullopt);
  TransactionQuery q;q.household_id=household_.id;
  const auto outgoing=service.list_asset_dto(source.id,q);
  const auto incoming=service.list_asset_dto(target.id,q);
  ASSERT_EQ(outgoing.size(),1u);ASSERT_EQ(incoming.size(),1u);
  EXPECT_EQ(outgoing[0].transaction.id,tx.id);EXPECT_EQ(outgoing[0].direction,DisplayDirection::Out);
  EXPECT_EQ(incoming[0].direction,DisplayDirection::In);
  EXPECT_EQ(outgoing[0].subtitle,"转至支付宝");
}

TEST_F(ServiceFixture, EditingTransferReversesOldEntriesAndAppliesNewOnes) {
  const Asset source=make_asset("银行卡",AssetType::Cash,10000);
  const Asset old_target=make_asset("支付宝",AssetType::Cash,1000);
  const Asset new_target=make_asset("微信",AssetType::Cash,500);
  TransactionService service(database_);
  const auto tx=service.transfer(household_.id,source.id,old_target.id,1000,"",std::nullopt);
  TransactionInput edited;edited.type=TransactionType::Transfer;edited.transaction_time=tx.transaction_time;
  edited.entries={{source.id,TransactionDirection::Out,1500},{new_target.id,TransactionDirection::In,1500}};
  service.update(tx.id,edited);
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,8500);
  EXPECT_EQ(AssetService(database_).get(old_target.id).current_balance,1000);
  EXPECT_EQ(AssetService(database_).get(new_target.id).current_balance,2000);
  const auto entries=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(entries.size(),2u);EXPECT_EQ(entries[0].amount,1500);EXPECT_EQ(entries[1].asset_id,new_target.id);
}

TEST_F(ServiceFixture, EditingTransferCanKeepAllAssetBalances) {
  const Asset source=make_asset("来源",AssetType::Cash,10000);
  const Asset old_target=make_asset("旧目标",AssetType::Cash,1000);
  const Asset new_target=make_asset("新目标",AssetType::Cash,500);
  TransactionService service(database_);
  const auto tx=service.transfer(household_.id,source.id,old_target.id,1000,"",std::nullopt);
  TransactionInput edited;edited.type=TransactionType::Transfer;edited.transaction_time=tx.transaction_time;
  edited.entries={{source.id,TransactionDirection::Out,1500},{new_target.id,TransactionDirection::In,1500}};
  service.update(tx.id,edited,false);
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,9000);
  EXPECT_EQ(AssetService(database_).get(old_target.id).current_balance,2000);
  EXPECT_EQ(AssetService(database_).get(new_target.id).current_balance,500);
  const auto entries=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(entries.size(),2u);
  EXPECT_EQ(entries[0].amount,1500);
  EXPECT_EQ(entries[1].asset_id,new_target.id);
  EXPECT_FALSE(entries[0].balance_before.has_value());
  EXPECT_FALSE(entries[1].balance_after.has_value());
}

TEST_F(ServiceFixture, MetadataOnlyEditKeepsEntriesAndBalances) {
  const Asset cash=make_asset("现金",AssetType::Cash,1000);
  TransactionService service(database_);
  const auto tx=service.record_income(household_.id,cash.id,std::nullopt,200,"",std::nullopt);
  const auto before=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(before.size(),1u);
  AssetService(database_).update_status(cash.id,AssetStatus::Closed);
  TransactionInput edited;edited.type=TransactionType::Income;edited.transaction_time=tx.transaction_time;
  edited.remark="修改备注";edited.entries={{cash.id,TransactionDirection::In,200}};
  EXPECT_NO_THROW(service.update(tx.id,edited));
  const auto after=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(after.size(),1u);
  EXPECT_EQ(after[0].id,before[0].id);
  EXPECT_EQ(after[0].balance_before,before[0].balance_before);
  EXPECT_EQ(after[0].balance_after,before[0].balance_after);
  EXPECT_EQ(AssetService(database_).get(cash.id).current_balance,1200);
  EXPECT_EQ(service.get(tx.id).remark,std::optional<std::string>("修改备注"));
}

TEST_F(ServiceFixture, LegacyCategoryCanBeKeptOrClearedWhileEditing) {
  const Asset cash=make_asset("现金",AssetType::Cash,1000);
  TransactionService service(database_);
  const auto tx=service.record_income(household_.id,cash.id,std::nullopt,200,"",std::nullopt);
  Statement legacy(database_,"UPDATE transactions SET category='旧分类' WHERE id=?;");
  legacy.bind(1,tx.id).run();
  TransactionInput edited;edited.type=TransactionType::Income;edited.transaction_time=tx.transaction_time;
  edited.entries={{cash.id,TransactionDirection::In,200}};
  edited.preserve_legacy_category=true;
  service.update(tx.id,edited);
  EXPECT_EQ(service.get(tx.id).category,std::optional<std::string>("旧分类"));
  edited.preserve_legacy_category=false;
  service.update(tx.id,edited);
  EXPECT_FALSE(service.get(tx.id).category.has_value());
}

TEST_F(ServiceFixture, AdjustmentDirectionCanChangeAndFailedEditRollsBack) {
  const Asset cash=make_asset("现金",AssetType::Cash,1000);
  TransactionService service(database_);
  const auto tx=service.record_adjustment(household_.id,cash.id,200,"",std::nullopt);
  TransactionInput edited;edited.type=TransactionType::Adjustment;edited.transaction_time=tx.transaction_time;
  edited.entries={{cash.id,TransactionDirection::Out,300}};
  service.update(tx.id,edited);
  EXPECT_EQ(AssetService(database_).get(cash.id).current_balance,700);
  edited.entries={{cash.id,TransactionDirection::Out,300},{cash.id,TransactionDirection::In,300}};
  EXPECT_THROW(service.update(tx.id,edited),ApiError);
  EXPECT_EQ(AssetService(database_).get(cash.id).current_balance,700);
  const auto entries=TransactionRepository(database_).entries(tx.id);
  ASSERT_EQ(entries.size(),1u);
  EXPECT_EQ(entries[0].direction,TransactionDirection::Out);
}

TEST_F(ServiceFixture, EditingInvestmentBuyUpdatesPrincipalAndBalances) {
  const Asset source=make_asset("付款",AssetType::Cash,5000);
  const Asset target=make_asset("基金",AssetType::StockFund,0);
  TransactionService service(database_);
  const auto tx=service.investment_buy(household_.id,source.id,target.id,4000,"",std::nullopt);
  TransactionInput edited;edited.type=TransactionType::Investment;edited.action=InvestmentAction::Buy;
  edited.transaction_time=tx.transaction_time;
  edited.entries={{source.id,TransactionDirection::Out,4500},{target.id,TransactionDirection::In,4500}};
  EXPECT_NO_THROW(service.update(tx.id,edited));
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,500);
  EXPECT_EQ(AssetService(database_).get(target.id).current_balance,4500);
  Statement detail(database_,"SELECT asset_id,principal FROM investment_transaction_details WHERE transaction_id=?;");
  detail.bind(1,tx.id);ASSERT_TRUE(detail.step());EXPECT_EQ(detail.get_int64(0),target.id);EXPECT_EQ(detail.get_int64(1),4500);
}

TEST_F(ServiceFixture, AssetWithTransactionEntriesCannotBeHardDeleted) {
  const Asset cash=make_asset("有流水资产",AssetType::Cash,1000);
  TransactionService(database_).record_income(household_.id,cash.id,std::nullopt,100,"",std::nullopt);
  EXPECT_THROW(AssetService(database_).remove(cash.id),ApiError);
  EXPECT_NO_THROW(AssetService(database_).get(cash.id));
}

TEST_F(ServiceFixture, InvalidMultiEntryTransactionWritesNothing) {
  const Asset source=make_asset("来源",AssetType::Cash,10000);
  const Asset target=make_asset("目标",AssetType::Cash,0);
  TransactionService service(database_);TransactionInput in;in.type=TransactionType::Transfer;
  in.entries={{source.id,TransactionDirection::Out,100},{target.id,TransactionDirection::In,99}};
  EXPECT_THROW(service.create(household_.id,in),ApiError);
  TransactionQuery q;q.household_id=household_.id;EXPECT_EQ(service.count(q),0);
  EXPECT_EQ(AssetService(database_).get(source.id).current_balance,10000);
  EXPECT_EQ(AssetService(database_).get(target.id).current_balance,0);
}

TEST_F(ServiceFixture, RejectsAmountsAndBalancesOutsideInt64RangeAtomically) {
  const Asset maxed=make_asset("达到余额上限",AssetType::Cash,std::numeric_limits<std::int64_t>::max());
  TransactionService service(database_);
  EXPECT_THROW(service.record_income(household_.id,maxed.id,std::nullopt,1,"",std::nullopt),ApiError);
  EXPECT_THROW(service.record_adjustment(household_.id,maxed.id,std::numeric_limits<std::int64_t>::min(),"",std::nullopt),ApiError);
  EXPECT_EQ(AssetService(database_).get(maxed.id).current_balance,std::numeric_limits<std::int64_t>::max());
  TransactionQuery query;query.household_id=household_.id;
  EXPECT_EQ(service.count(query),0);
}

// 验证删除不存在的流水抛 not_found。
TEST_F(ServiceFixture, DeleteMissingTransactionRejected) {
  TransactionService transactions(database_);
  EXPECT_THROW(transactions.remove(999999), ApiError);
}

// 验证近 N 个月收支趋势：返回 N 个月、当前月在最后，缺月补零。
TEST_F(ServiceFixture, MonthlyTrendFillsMissingMonths) {
  const Asset cash = make_asset("活期", AssetType::Cash, 0);
  TransactionService transactions(database_);
  const std::string at = business_noon_utc();
  transactions.record_income(household_.id, cash.id, "工资", 1000000, at, std::nullopt);
  transactions.record_expense(household_.id, cash.id, "餐饮", 300000, at, std::nullopt);

  StatisticsService statistics(database_);
  const auto trend = statistics.monthly(household_.id, 6);
  ASSERT_EQ(trend.size(), 6u);

  const std::string current = time_util::business_today().substr(0, 7);
  EXPECT_EQ(trend.back().month, current);
  EXPECT_EQ(trend.back().income, 1000000);
  EXPECT_EQ(trend.back().expense, 300000);
  EXPECT_EQ(trend.back().balance(), 700000);

  for (std::size_t i = 0; i + 1 < trend.size(); ++i) {
    EXPECT_EQ(trend[i].income, 0);
    EXPECT_EQ(trend[i].expense, 0);
  }
}

// 验证近 N 个月的月份参数越界被拒绝。
TEST_F(ServiceFixture, MonthlyRejectsInvalidRange) {
  StatisticsService statistics(database_);
  EXPECT_THROW(statistics.monthly(household_.id, 0), ApiError);
  EXPECT_THROW(statistics.monthly(household_.id, 37), ApiError);
}

// 区间统计的 from/to 按业务时区解释：UTC 前一晚的流水应计入业务当天。
TEST_F(ServiceFixture, PeriodInterpretsBoundsInBusinessTimezone) {
  time_util::set_business_timezone("Asia/Shanghai");  // UTC+8
  const Asset cash = make_asset("活期", AssetType::Cash, 0);
  TransactionService transactions(database_);
  // UTC 2026-09-21 23:00 == 业务（Asia/Shanghai）2026-09-22 07:00
  transactions.record_income(household_.id, cash.id, "工资", 100000,
                             "2026-09-21 23:00:00", std::nullopt);
  StatisticsService statistics(database_);

  const auto day = statistics.period(household_.id, "2026-09-22 00:00:00",
                                     "2026-09-22 23:59:59", std::nullopt);
  EXPECT_EQ(day.income, 100000);

  const auto previous = statistics.period(household_.id, "2026-09-21 00:00:00",
                                          "2026-09-21 23:59:59", std::nullopt);
  EXPECT_EQ(previous.income, 0);
}

// 在夹具账户下创建一个债券基金资产，返回聚合结果。
static AssetBundle make_bond_fund(Database& database, std::int64_t household_id,
                                  std::int64_t account_id, const std::string& purchase_date,
                                  HoldingMode mode, std::int64_t period_days) {
  AssetService assets(database);
  AssetCreateInput input;
  input.account_id = account_id;
  input.name = "债券基金";
  input.asset_type = AssetType::BondFund;
  input.opening_balance = 10000000;
  BondFundDetail detail;
  detail.purchase_date = purchase_date;
  detail.holding_mode = mode;
  detail.holding_period_days = period_days;
  input.bond_fund = detail;
  return assets.create(household_id, input);
}

// 持有期债基：系统按「购买日期 + 持有周期」计算首次可赎回日，next_redeem_date 恒为空。
TEST_F(ServiceFixture, BondFundMinHoldingComputesFirstRedeemDate) {
  const AssetBundle bundle = make_bond_fund(database_, household_.id, account_.id,
                                            "2026-09-22", HoldingMode::MinHolding, 90);
  ASSERT_TRUE(bundle.bond_fund.has_value());
  EXPECT_EQ(bundle.bond_fund->first_redeem_date.value_or(""), "2026-12-21");
  EXPECT_FALSE(bundle.bond_fund->next_redeem_date.has_value());
}

// 滚动持有债基：创建时 next_redeem_date 初始化为 first_redeem_date。
TEST_F(ServiceFixture, BondFundRollingInitializesNextRedeemDate) {
  const AssetBundle bundle = make_bond_fund(database_, household_.id, account_.id,
                                            "2026-09-22", HoldingMode::Rolling, 90);
  ASSERT_TRUE(bundle.bond_fund.has_value());
  EXPECT_EQ(bundle.bond_fund->first_redeem_date.value_or(""), "2026-12-21");
  EXPECT_EQ(bundle.bond_fund->next_redeem_date.value_or(""), "2026-12-21");
}

// 每日维护把已错过的赎回日推进到今天（可跨多个周期），且具备幂等性。
TEST_F(ServiceFixture, BondFundRollingMaintenanceAdvancesPastDates) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle = make_bond_fund(database_, household_.id, account_.id,
                                            time_util::add_days(today, -200),
                                            HoldingMode::Rolling, 90);

  DailyAssetMaintenanceService maintenance(database_);
  maintenance.run();

  AssetService assets(database_);
  const auto advanced = assets.get_bundle(bundle.asset.id);
  ASSERT_TRUE(advanced.bond_fund->next_redeem_date.has_value());
  // 推进后不应早于今天（today > next 才继续加周期）。
  EXPECT_GE(*advanced.bond_fund->next_redeem_date, today);

  // 再次执行结果不变。
  maintenance.run();
  const auto again = assets.get_bundle(bundle.asset.id);
  EXPECT_EQ(again.bond_fund->next_redeem_date, advanced.bond_fund->next_redeem_date);
}

// 赎回日正好是今天时必须保持不变：当天仍是有效赎回日，不能滚入下一期。
TEST_F(ServiceFixture, BondFundRollingKeepsTodaysRedeemDate) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle = make_bond_fund(database_, household_.id, account_.id,
                                            time_util::add_days(today, -90),
                                            HoldingMode::Rolling, 90);
  EXPECT_EQ(bundle.bond_fund->next_redeem_date.value_or(""), today);

  DailyAssetMaintenanceService maintenance(database_);
  maintenance.run();

  AssetService assets(database_);
  const auto after = assets.get_bundle(bundle.asset.id);
  EXPECT_EQ(after.bond_fund->next_redeem_date.value_or(""), today);
}

// 添加时维护：创建滚动债基时带 maintain_on_create，过期赎回日立即推进到未来。
TEST_F(ServiceFixture, BondFundCreateMaintainsPastRedeemDate) {
  const std::string today = time_util::business_today();
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "债券基金";
  input.asset_type = AssetType::BondFund;
  input.opening_balance = 10000000;
  input.maintain_on_create = true;
  BondFundDetail detail;
  detail.purchase_date = time_util::add_days(today, -200);
  detail.holding_mode = HoldingMode::Rolling;
  detail.holding_period_days = 90;
  input.bond_fund = detail;

  const auto bundle = assets.create(household_.id, input);
  ASSERT_TRUE(bundle.bond_fund.has_value());
  ASSERT_TRUE(bundle.bond_fund->next_redeem_date.has_value());
  EXPECT_GE(*bundle.bond_fund->next_redeem_date, today);
}

// 不带 maintain_on_create 时保持原样，由每日维护或手动维护后续处理。
TEST_F(ServiceFixture, BondFundCreateWithoutMaintainKeepsPastRedeemDate) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle = make_bond_fund(database_, household_.id, account_.id,
                                            time_util::add_days(today, -200),
                                            HoldingMode::Rolling, 90);
  ASSERT_TRUE(bundle.bond_fund->next_redeem_date.has_value());
  EXPECT_LT(*bundle.bond_fund->next_redeem_date, today);
}

// 添加时维护预览：过期滚动债基返回 required 与推进前后值；未过期则不需要维护。
TEST_F(ServiceFixture, BondFundCreateMaintenancePreview) {
  const std::string today = time_util::business_today();
  AssetService assets(database_);

  AssetCreateInput past;
  past.asset_type = AssetType::BondFund;
  BondFundDetail detail;
  detail.purchase_date = time_util::add_days(today, -200);
  detail.holding_mode = HoldingMode::Rolling;
  detail.holding_period_days = 90;
  past.bond_fund = detail;
  const auto preview = assets.preview_create_maintenance(past);
  EXPECT_TRUE(preview.required);
  ASSERT_EQ(preview.changes.size(), 1u);
  EXPECT_EQ(preview.changes[0].field, "next_redeem_date");
  EXPECT_GE(preview.changes[0].after, today);

  AssetCreateInput future;
  future.asset_type = AssetType::BondFund;
  detail.purchase_date = today;  // 首期赎回日在未来，无需维护
  future.bond_fund = detail;
  EXPECT_FALSE(assets.preview_create_maintenance(future).required);
}

// 每日维护当天只执行一次，并在 system_state 记录最近执行日期。
TEST_F(ServiceFixture, DailyMaintenanceRunsOncePerDay) {
  DailyAssetMaintenanceService maintenance(database_);
  EXPECT_TRUE(maintenance.run_if_due());
  EXPECT_FALSE(maintenance.run_if_due());

  SystemStateRepository states(database_);
  EXPECT_EQ(states.get("daily_maintenance_last_run").value_or(""),
            time_util::business_today());
}

// 持有周期必须为正数。
TEST_F(ServiceFixture, BondFundRejectsNonPositiveHoldingPeriod) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "债券基金";
  input.asset_type = AssetType::BondFund;
  BondFundDetail detail;
  detail.purchase_date = "2026-09-22";
  detail.holding_mode = HoldingMode::Rolling;
  detail.holding_period_days = 0;
  input.bond_fund = detail;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);
}

// 在夹具账户下创建定期存款，返回聚合结果。maturity_override 非空则手工指定到期日。
static AssetBundle make_term_deposit(Database& database, std::int64_t household_id,
                                     std::int64_t account_id, const std::string& start_date,
                                     std::int64_t term_value, TermUnit unit,
                                     bool auto_rollover,
                                     const std::string& maturity_override = "") {
  AssetService assets(database);
  AssetCreateInput input;
  input.account_id = account_id;
  input.name = "定期存款";
  input.asset_type = AssetType::TermDeposit;
  input.opening_balance = 10000000;
  TermDepositDetail detail;
  detail.annual_interest_rate = 15000;
  detail.start_date = start_date;
  detail.term_value = term_value;
  detail.term_unit = unit;
  detail.auto_rollover = auto_rollover;
  if (!maturity_override.empty()) {
    detail.maturity_date = maturity_override;
  }
  input.term_deposit = detail;
  return assets.create(household_id, input);
}

// 定期存款创建时按「起息日 + 存期（自然年/月）」计算到期日。
TEST_F(ServiceFixture, TermDepositComputesMaturityDate) {
  const AssetBundle bundle =
      make_term_deposit(database_, household_.id, account_.id, "2026-09-22", 3,
                        TermUnit::Year, false);
  ASSERT_TRUE(bundle.term_deposit.has_value());
  EXPECT_EQ(bundle.term_deposit->maturity_date.value_or(""), "2029-09-22");
  EXPECT_EQ(bundle.term_deposit->start_date.value_or(""), "2026-09-22");
}

// 月末/闰年规则：目标月没有对应日时落到该月最后一天。
TEST_F(ServiceFixture, TermDepositClampsToMonthEnd) {
  const AssetBundle jan =
      make_term_deposit(database_, household_.id, account_.id, "2026-01-31", 1,
                        TermUnit::Month, false);
  EXPECT_EQ(jan.term_deposit->maturity_date.value_or(""), "2026-02-28");

  const AssetBundle leap =
      make_term_deposit(database_, household_.id, account_.id, "2024-02-29", 1,
                        TermUnit::Year, false);
  EXPECT_EQ(leap.term_deposit->maturity_date.value_or(""), "2025-02-28");
}

// 起息日 / 存期 / 存期单位必填。
TEST_F(ServiceFixture, TermDepositRejectsMissingTerm) {
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "定期存款";
  input.asset_type = AssetType::TermDeposit;
  TermDepositDetail detail;
  detail.start_date = "2026-09-22";
  detail.term_value = 3;
  // 缺少 term_unit
  input.term_deposit = detail;
  EXPECT_THROW(assets.create(household_.id, input), ApiError);
}

// 自动续存：到期即进入下一期（today >= maturity），停机多年也可一次推进；
// start_date 同步推进为当前存期起始日；任务幂等。
TEST_F(ServiceFixture, TermDepositAutoRolloverAdvancesPeriods) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle =
      make_term_deposit(database_, household_.id, account_.id, "2020-01-01", 1,
                        TermUnit::Year, true);
  EXPECT_EQ(bundle.term_deposit->maturity_date.value_or(""), "2021-01-01");

  DailyAssetMaintenanceService maintenance(database_);
  maintenance.run();

  AssetService assets(database_);
  const auto advanced = assets.get_bundle(bundle.asset.id);
  ASSERT_TRUE(advanced.term_deposit->maturity_date.has_value());
  EXPECT_GT(*advanced.term_deposit->maturity_date, today);
  // start_date 是当前存期起点（上一期到期日），且与到期日相差一个存期。
  EXPECT_LE(advanced.term_deposit->start_date.value_or(""), today);
  EXPECT_EQ(time_util::add_term(*advanced.term_deposit->start_date, 1, TermUnit::Year),
            *advanced.term_deposit->maturity_date);

  // 幂等：再次执行不再变化。
  maintenance.run();
  const auto again = assets.get_bundle(bundle.asset.id);
  EXPECT_EQ(again.term_deposit->maturity_date, advanced.term_deposit->maturity_date);
  EXPECT_EQ(again.term_deposit->start_date, advanced.term_deposit->start_date);
}

// 到期当天即续存：自动续存用 today >= maturity 判定（与滚动债基不同）。
TEST_F(ServiceFixture, TermDepositRollsOnMaturityDay) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle =
      make_term_deposit(database_, household_.id, account_.id,
                        time_util::add_days(today, -1), 1, TermUnit::Day, true);
  EXPECT_EQ(bundle.term_deposit->maturity_date.value_or(""), today);

  DailyAssetMaintenanceService maintenance(database_);
  maintenance.run();

  AssetService assets(database_);
  const auto after = assets.get_bundle(bundle.asset.id);
  EXPECT_EQ(after.term_deposit->start_date.value_or(""), today);
  EXPECT_EQ(after.term_deposit->maturity_date.value_or(""), time_util::add_days(today, 1));
}

// 不自动续存：即使已到期也不修改日期，状态由日期动态推导（MATURED）。
TEST_F(ServiceFixture, TermDepositNoAutoRolloverKeepsMaturity) {
  const std::string today = time_util::business_today();
  const AssetBundle bundle =
      make_term_deposit(database_, household_.id, account_.id,
                        time_util::add_days(today, -1), 1, TermUnit::Day, false);
  EXPECT_EQ(bundle.term_deposit->maturity_date.value_or(""), today);

  DailyAssetMaintenanceService maintenance(database_);
  maintenance.run();

  AssetService assets(database_);
  const auto after = assets.get_bundle(bundle.asset.id);
  EXPECT_EQ(after.term_deposit->maturity_date.value_or(""), today);
  EXPECT_EQ(after.term_deposit->start_date.value_or(""), time_util::add_days(today, -1));
}

// 添加时维护：自动续存定存创建时一次性推进到当前存期，起息日同步为当前存期起点。
TEST_F(ServiceFixture, TermDepositCreateMaintainsAutoRollover) {
  const std::string today = time_util::business_today();
  AssetService assets(database_);
  AssetCreateInput input;
  input.account_id = account_.id;
  input.name = "定期存款";
  input.asset_type = AssetType::TermDeposit;
  input.opening_balance = 10000000;
  input.maintain_on_create = true;
  TermDepositDetail detail;
  detail.start_date = "2020-01-01";
  detail.term_value = 1;
  detail.term_unit = TermUnit::Year;
  detail.auto_rollover = true;
  input.term_deposit = detail;

  const auto bundle = assets.create(household_.id, input);
  ASSERT_TRUE(bundle.term_deposit.has_value());
  ASSERT_TRUE(bundle.term_deposit->maturity_date.has_value());
  EXPECT_GT(*bundle.term_deposit->maturity_date, today);
  EXPECT_LE(bundle.term_deposit->start_date.value_or(""), today);
  EXPECT_EQ(
      time_util::add_term(*bundle.term_deposit->start_date, 1, TermUnit::Year),
      *bundle.term_deposit->maturity_date);
}

// 维护预览：列出将要推进的资产变更，且不修改任何数据。
TEST_F(ServiceFixture, MaintenancePreviewListsChangesWithoutPersisting) {
  const std::string today = time_util::business_today();
  const AssetBundle bond_fund = make_bond_fund(database_, household_.id, account_.id,
                                          time_util::add_days(today, -200),
                                          HoldingMode::Rolling, 90);

  DailyAssetMaintenanceService maintenance(database_);
  const auto plan = maintenance.preview();
  EXPECT_TRUE(plan.required);
  ASSERT_EQ(plan.changes.size(), 1u);
  EXPECT_EQ(plan.changes[0].asset_id, bond_fund.asset.id);
  EXPECT_EQ(plan.changes[0].asset_type, "BOND_FUND");
  EXPECT_EQ(plan.changes[0].field, "next_redeem_date");
  EXPECT_GE(plan.changes[0].after, today);

  // 预览不落库：数据库中仍是原值。
  AssetService assets(database_);
  const auto unchanged = assets.get_bundle(bond_fund.asset.id);
  EXPECT_LT(*unchanged.bond_fund->next_redeem_date, today);
}

// 手动维护 run 返回各类被推进的条数，且幂等。
TEST_F(ServiceFixture, MaintenanceRunReportsCounts) {
  const std::string today = time_util::business_today();
  make_bond_fund(database_, household_.id, account_.id, time_util::add_days(today, -200),
                 HoldingMode::Rolling, 90);
  make_term_deposit(database_, household_.id, account_.id, "2020-01-01", 1,
                    TermUnit::Year, true);

  DailyAssetMaintenanceService maintenance(database_);
  const auto result = maintenance.run();
  EXPECT_EQ(result.bond_funds, 1);
  EXPECT_EQ(result.term_deposits, 1);

  // 再次执行无变更。
  const auto again = maintenance.run();
  EXPECT_EQ(again.bond_funds, 0);
  EXPECT_EQ(again.term_deposits, 0);
}

}  // namespace

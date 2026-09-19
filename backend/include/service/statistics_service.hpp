// 统计服务：家庭资产总览与指定区间的收支统计（口径以 Repository 为准）。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "repository/household_repository.hpp"
#include "repository/statistics_repository.hpp"

namespace wt {

class Database;

// 家庭总览：金额单位为分。负债以负数存于 current_balance，
// total_liabilities 取其绝对值；net_worth 直接由全部资产余额求和得到
// （在负债恒为负的前提下等价于 total_assets - total_liabilities）。
// by_* 为各维度资产余额分布；month_* 为指定自然月的收支汇总。
struct HouseholdOverview {
  std::int64_t total_assets = 0;
  std::int64_t total_liabilities = 0;
  std::int64_t net_worth = 0;
  std::int64_t month_income = 0;
  std::int64_t month_expense = 0;
  std::int64_t month_balance = 0;
  std::vector<NamedAmount> by_member;
  std::vector<NamedAmount> by_account;
  std::vector<TypeAmount> by_type;
};

// 区间统计：收支只统计 INCOME/EXPENSE（转账不计入，避免重复计）。
// by_member 是该区间内每个成员的净收支（收入 - 支出）。
struct PeriodStatistics {
  std::string from_time;
  std::string to_time;
  std::int64_t income = 0;
  std::int64_t expense = 0;
  std::int64_t balance = 0;
  std::vector<NamedAmount> by_member;
  std::vector<CategoryAmount> income_categories;
  std::vector<CategoryAmount> expense_categories;
};

// 统计服务：只读聚合，不修改任何数据。所有金额为分，时间为 UTC 字符串。
// 统计一律只计入 status=ACTIVE 的资产与 status=NORMAL 的流水。
class StatisticsService {
 public:
  explicit StatisticsService(Database& database)
      : households_(database), statistics_(database) {}

  // 家庭总览：先取当前资产/负债/净值与各维度分布，再把 year-month 这个
  // 自然月换算成 [月初 00:00:00, 月末 23:59:59] 闭区间统计当月收支。
  // 家庭不存在抛 not_found，年/月越界抛 invalid_request。
  HouseholdOverview overview(std::int64_t household_id, int year, int month);

  // 区间统计：from_time/to_time 为含端点的 UTC 时间，from > to 抛
  // invalid_request。member_id 可选，仅用于过滤总收支；分类与成员分布
  // 仍按整个家庭统计。
  PeriodStatistics period(std::int64_t household_id, const std::string& from_time,
                          const std::string& to_time,
                          std::optional<std::int64_t> member_id);

 private:
  // 前置校验：家庭必须存在，否则抛 not_found。
  void require_household(std::int64_t household_id);

  HouseholdRepository households_;
  StatisticsRepository statistics_;
};

}  // namespace wt

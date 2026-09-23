// 统计服务实现：家庭资产总览与收支区间统计，负责月份区间换算与口径校验。
#include "service/statistics_service.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

// 闰年判断：用于确定 2 月天数，避免月末日期算错导致漏统计。
bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 返回某年某月的天数（2 月按平/闰年区分）。
int days_in_month(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return kDays[month - 1];
}

// 计算业务时区下「某年某月」的闭区间 [首日 00:00:00, 末日 23:59:59]，
// 再换算成 UTC 字符串，用于查询以 UTC 存储的 transaction_time。
// 每个月的边界单独换算，因此即使业务时区有夏令时也正确。
std::pair<std::string, std::string> month_utc_range(int year, int month) {
  char from[32];
  char to[32];
  std::snprintf(from, sizeof(from), "%04d-%02d-01 00:00:00", year, month);
  std::snprintf(to, sizeof(to), "%04d-%02d-%02d 23:59:59", year, month,
               days_in_month(year, month));
  return {time_util::business_to_utc(from), time_util::business_to_utc(to)};
}

}  // namespace

void StatisticsService::require_household(std::int64_t household_id) {
  if (!households_.find_by_id(household_id).has_value()) {
    throw not_found("household not found");
  }
}

HouseholdOverview StatisticsService::overview(std::int64_t household_id, int year,
                                              int month) {
  std::scoped_lock lock(database_.mutex());
  require_household(household_id);
  // 年份限制在 4 位可表示范围，月份 1..12，防止生成非法时间串。
  if (year < 1970 || year > 9999) {
    throw invalid_request("year out of range");
  }
  if (month < 1 || month > 12) {
    throw invalid_request("month out of range");
  }

  // 资产口径：只统计 ACTIVE 资产。total_assets 不含负债；
  // total_liabilities 为负债余额的绝对值；net_worth 为全部资产余额之和。
  HouseholdOverview overview;
  overview.total_assets = statistics_.total_assets(household_id);
  overview.total_liabilities = statistics_.total_liabilities(household_id);
  overview.net_worth = statistics_.total_balance(household_id);
  overview.by_member = statistics_.assets_by_member(household_id);
  overview.by_account = statistics_.assets_by_account(household_id);
  overview.by_type = statistics_.assets_by_type(household_id);

  // 业务时区下的「年-月」闭区间，换算成 UTC 后查询（transaction_time 存 UTC）。
  const auto [from, to] = month_utc_range(year, month);
  const auto month_summary =
      statistics_.income_expense(household_id, std::nullopt, from, to);
  overview.month_income = month_summary.income;
  overview.month_expense = month_summary.expense;
  overview.month_balance = month_summary.balance();
  return overview;
}

PeriodStatistics StatisticsService::period(
    std::int64_t household_id, const std::string& from_time, const std::string& to_time,
    std::optional<std::int64_t> member_id) {
  std::scoped_lock lock(database_.mutex());
  require_household(household_id);
  PeriodStatistics result;
  // from/to 由用户按业务时区填写；仅校验格式，随后换算成 UTC 区间查询。
  result.from_time = time_util::require_datetime(from_time, "from");
  result.to_time = time_util::require_datetime(to_time, "to");
  if (result.from_time > result.to_time) {
    throw invalid_request("from must not be after to");
  }
  const std::string from_utc = time_util::business_to_utc(result.from_time);
  const std::string to_utc = time_util::business_to_utc(result.to_time);

  // 收支口径：只汇总 INCOME/EXPENSE，转账/调整不计入，避免内部流转被
  // 误当成收入或支出。member_id 只作用于总收支，不改变下面的分布统计。
  const auto summary =
      statistics_.income_expense(household_id, member_id, from_utc, to_utc);
  result.income = summary.income;
  result.expense = summary.expense;
  result.balance = summary.balance();
  // 成员分布固定按整个家庭统计（不随后面的 member_id 过滤而收窄）。
  result.by_member =
      statistics_.income_expense_by_member(household_id, from_utc, to_utc);
  result.expense_categories =
      statistics_.expense_by_category(household_id, from_utc, to_utc);
  result.income_categories =
      statistics_.income_by_category(household_id, from_utc, to_utc);
  return result;
}

std::vector<MonthlyIncomeExpense> StatisticsService::monthly(std::int64_t household_id,
                                                             int months) {
  std::scoped_lock lock(database_.mutex());
  require_household(household_id);
  if (months < 1 || months > 36) {
    throw invalid_request("months must be between 1 and 36");
  }

  // 以业务时区下的当前自然月为终点，向前取 N 个月，先收集再反转成升序。
  const std::string today = time_util::business_today();
  int year = std::stoi(today.substr(0, 4));
  int month = std::stoi(today.substr(5, 2));

  std::vector<std::pair<int, int>> month_list;  // (year, month)
  for (int i = 0; i < months; ++i) {
    month_list.emplace_back(year, month);
    if (--month == 0) {
      month = 12;
      --year;
    }
  }
  std::reverse(month_list.begin(), month_list.end());

  // 逐月按业务时区边界查询收支；每月边界单独换算成 UTC，跨夏令时也正确。
  // 单次聚合查询足够轻量，N 最大 36，无需在 SQL 里做时区分组。
  std::vector<MonthlyIncomeExpense> result;
  result.reserve(month_list.size());
  for (const auto& [y, m] : month_list) {
    const auto [from, to] = month_utc_range(y, m);
    const auto summary = statistics_.income_expense(household_id, std::nullopt, from, to);
    MonthlyIncomeExpense row;
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d", y, m);
    row.month = buffer;
    row.income = summary.income;
    row.expense = summary.expense;
    result.push_back(row);
  }
  return result;
}

}  // namespace wt

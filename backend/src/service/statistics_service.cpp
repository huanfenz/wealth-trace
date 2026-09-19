#include "service/statistics_service.hpp"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int days_in_month(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return kDays[month - 1];
}

std::string format_datetime(int year, int month, int day) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d 00:00:00", year, month, day);
  return buffer;
}

std::string format_datetime_end(int year, int month, int day) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d 23:59:59", year, month, day);
  return buffer;
}

}  // namespace

void StatisticsService::require_household(std::int64_t household_id) {
  if (!households_.find_by_id(household_id).has_value()) {
    throw not_found("household not found");
  }
}

HouseholdOverview StatisticsService::overview(std::int64_t household_id, int year,
                                              int month) {
  require_household(household_id);
  if (year < 1970 || year > 9999) {
    throw invalid_request("year out of range");
  }
  if (month < 1 || month > 12) {
    throw invalid_request("month out of range");
  }

  HouseholdOverview overview;
  overview.total_assets = statistics_.total_assets(household_id);
  overview.total_liabilities = statistics_.total_liabilities(household_id);
  overview.net_worth = statistics_.total_balance(household_id);
  overview.by_member = statistics_.assets_by_member(household_id);
  overview.by_account = statistics_.assets_by_account(household_id);
  overview.by_type = statistics_.assets_by_type(household_id);

  const std::string from = format_datetime(year, month, 1);
  const std::string to = format_datetime_end(year, month, days_in_month(year, month));
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
  require_household(household_id);
  PeriodStatistics result;
  result.from_time = time_util::require_datetime(from_time, "from");
  result.to_time = time_util::require_datetime(to_time, "to");
  if (result.from_time > result.to_time) {
    throw invalid_request("from must not be after to");
  }

  const auto summary =
      statistics_.income_expense(household_id, member_id, result.from_time, result.to_time);
  result.income = summary.income;
  result.expense = summary.expense;
  result.balance = summary.balance();
  result.by_member =
      statistics_.income_expense_by_member(household_id, result.from_time, result.to_time);
  result.expense_categories =
      statistics_.expense_by_category(household_id, result.from_time, result.to_time);
  result.income_categories =
      statistics_.income_by_category(household_id, result.from_time, result.to_time);
  return result;
}

}  // namespace wt

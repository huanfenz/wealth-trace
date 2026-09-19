#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "repository/household_repository.hpp"
#include "repository/statistics_repository.hpp"

namespace wt {

class Database;

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

class StatisticsService {
 public:
  explicit StatisticsService(Database& database)
      : households_(database), statistics_(database) {}

  HouseholdOverview overview(std::int64_t household_id, int year, int month);
  PeriodStatistics period(std::int64_t household_id, const std::string& from_time,
                          const std::string& to_time,
                          std::optional<std::int64_t> member_id);

 private:
  void require_household(std::int64_t household_id);

  HouseholdRepository households_;
  StatisticsRepository statistics_;
};

}  // namespace wt

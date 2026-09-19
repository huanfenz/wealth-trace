#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/enums.hpp"

namespace wt {

class Database;

struct NamedAmount {
  std::int64_t id = 0;
  std::string name;
  std::int64_t amount = 0;
};

struct TypeAmount {
  AssetType type = AssetType::Other;
  std::int64_t amount = 0;
};

struct CategoryAmount {
  std::string category;
  std::int64_t amount = 0;
};

struct IncomeExpenseSummary {
  std::int64_t income = 0;
  std::int64_t expense = 0;
  std::int64_t balance() const { return income - expense; }
};

class StatisticsRepository {
 public:
  explicit StatisticsRepository(Database& database) : database_(database) {}

  // Net worth components (minor units). Liabilities are stored as negative
  // current_balance values, so `total_balance` is the household net worth.
  std::int64_t total_assets(std::int64_t household_id);
  std::int64_t total_liabilities(std::int64_t household_id);
  std::int64_t total_balance(std::int64_t household_id);
  std::int64_t member_balance(std::int64_t household_id, std::int64_t member_id);

  std::vector<NamedAmount> assets_by_member(std::int64_t household_id);
  std::vector<NamedAmount> assets_by_account(std::int64_t household_id);
  std::vector<TypeAmount> assets_by_type(std::int64_t household_id);

  IncomeExpenseSummary income_expense(std::int64_t household_id,
                                      std::optional<std::int64_t> member_id,
                                      const std::string& from_time,
                                      const std::string& to_time);
  std::vector<NamedAmount> income_expense_by_member(std::int64_t household_id,
                                                    const std::string& from_time,
                                                    const std::string& to_time);
  std::vector<CategoryAmount> expense_by_category(std::int64_t household_id,
                                                  const std::string& from_time,
                                                  const std::string& to_time);
  std::vector<CategoryAmount> income_by_category(std::int64_t household_id,
                                                 const std::string& from_time,
                                                 const std::string& to_time);

 private:
  Database& database_;
};

}  // namespace wt

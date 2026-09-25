#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/recurring_investment.hpp"
#include "repository/asset_repository.hpp"
#include "repository/recurring_investment_repository.hpp"
#include "repository/transaction_repository.hpp"

namespace wt {
class Database;

struct RecurringInvestmentInput {
  std::int64_t target_asset_id = 0;
  std::int64_t source_asset_id = 0;
  std::int64_t amount = 0;
  std::string frequency;
  std::optional<int> weekday;
  std::optional<int> month_day;
  std::string start_date;
};

class RecurringInvestmentService {
 public:
  explicit RecurringInvestmentService(Database& db)
      : database_(db), plans_(db), assets_(db), transactions_(db) {}
  std::vector<RecurringInvestmentPlan> list(std::int64_t household_id);
  std::vector<RecurringInvestmentExecution> executions(std::int64_t plan_id);
  RecurringInvestmentPlan create(std::int64_t household_id, const RecurringInvestmentInput& input);
  RecurringInvestmentPlan update(std::int64_t id, const RecurringInvestmentInput& input);
  RecurringInvestmentPlan set_status(std::int64_t id, const std::string& status);
  void remove(std::int64_t id);
  RecurringInvestmentExecution retry(std::int64_t execution_id);
  RecurringInvestmentExecution execute_now(std::int64_t plan_id);
  std::int64_t process_due();

 private:
  RecurringInvestmentPlan normalize(std::int64_t household_id,
      std::int64_t owner_member_id, const RecurringInvestmentInput& input,
      const std::string& first_date) const;
  void validate_assets(std::int64_t household_id, const RecurringInvestmentInput& input,
                       std::int64_t* owner_member_id);
  void process_plan(RecurringInvestmentPlan plan, const std::string& today);
  void execute(RecurringInvestmentExecution& execution,
               const std::optional<std::string>& next_due = std::nullopt);
  RecurringInvestmentPlan require_plan(std::int64_t id);

  Database& database_;
  RecurringInvestmentRepository plans_;
  AssetRepository assets_;
  TransactionRepository transactions_;
};
}  // namespace wt

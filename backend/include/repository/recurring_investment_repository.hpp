#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/recurring_investment.hpp"

namespace wt {
class Database;

class RecurringInvestmentRepository {
 public:
  explicit RecurringInvestmentRepository(Database& database) : database_(database) {}
  std::int64_t create(const RecurringInvestmentPlan& plan);
  std::optional<RecurringInvestmentPlan> find(std::int64_t id);
  std::vector<RecurringInvestmentPlan> list(std::int64_t household_id);
  std::vector<RecurringInvestmentPlan> list_due(const std::string& today);
  bool has_execution(std::int64_t plan_id, const std::string& date);
  bool update(const RecurringInvestmentPlan& plan);
  bool update_due(std::int64_t id, const std::string& date, const std::string& updated_at);
  bool set_status(std::int64_t id, const std::string& status, const std::string& updated_at);
  std::int64_t create_execution(const RecurringInvestmentExecution& execution);
  std::optional<RecurringInvestmentExecution> find_execution(std::int64_t id);
  std::vector<RecurringInvestmentExecution> list_executions(std::int64_t plan_id);
  bool update_execution(const RecurringInvestmentExecution& execution);
  bool mark_reversed(std::int64_t transaction_id, const std::string& updated_at);

 private:
  Database& database_;
};
}  // namespace wt

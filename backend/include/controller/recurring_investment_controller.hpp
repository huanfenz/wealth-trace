#pragma once
#include "crow.h"
#include "service/recurring_investment_service.hpp"

namespace wt {
class Database;
class RecurringInvestmentController {
 public:
  explicit RecurringInvestmentController(Database& db): service_(db) {}
  void register_routes(crow::SimpleApp& app);
 private:
  RecurringInvestmentService service_;
};
}

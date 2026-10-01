#pragma once
#include "crow.h"

#include "controller/auth_middleware.hpp"
#include "service/recurring_investment_service.hpp"

namespace wt {
class Database;
class RecurringInvestmentController {
 public:
  explicit RecurringInvestmentController(Database& db): service_(db) {}
  void register_routes(App& app);
 private:
  RecurringInvestmentService service_;
};
}

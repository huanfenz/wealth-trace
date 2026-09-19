#pragma once

#include "crow.h"

#include "service/household_service.hpp"

namespace wt {

class Database;

class HouseholdController {
 public:
  explicit HouseholdController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  HouseholdService service_;
};

}  // namespace wt

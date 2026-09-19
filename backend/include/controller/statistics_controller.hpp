#pragma once

#include "crow.h"

#include "service/statistics_service.hpp"

namespace wt {

class Database;

class StatisticsController {
 public:
  explicit StatisticsController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  StatisticsService service_;
};

}  // namespace wt

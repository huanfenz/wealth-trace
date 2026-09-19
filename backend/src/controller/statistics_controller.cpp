#include "controller/statistics_controller.hpp"

#include <string>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/serialization.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

int current_year() {
  const std::string today = time_util::today_iso8601();
  return std::stoi(today.substr(0, 4));
}

int current_month() {
  const std::string today = time_util::today_iso8601();
  return std::stoi(today.substr(5, 2));
}

}  // namespace

void StatisticsController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/households/<int>/statistics/overview").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const int year = http::query_int(request, "year", current_year());
          const int month = http::query_int(request, "month", current_month());
          return dto::to_json(service_.overview(id, year, month));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/statistics/period").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto from = http::query_string(request, "from");
          const auto to = http::query_string(request, "to");
          if (!from.has_value() || !to.has_value()) {
            throw invalid_request("from and to query parameters are required");
          }
          const auto member_id = http::query_int64(request, "owner_member_id");
          return dto::to_json(service_.period(id, *from, *to, member_id));
        });
      });
}

}  // namespace wt

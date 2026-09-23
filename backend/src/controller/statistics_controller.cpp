// 统计控制器实现：解析请求 -> 调用 StatisticsService -> 序列化 JSON。
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

// 从业务日期取当前年份，作为 overview 的默认年。
int current_year() {
  const std::string today = time_util::business_today();
  return std::stoi(today.substr(0, 4));
}

// 从业务日期取当前月份，作为 overview 的默认月。
int current_month() {
  const std::string today = time_util::business_today();
  return std::stoi(today.substr(5, 2));
}

}  // namespace

void StatisticsController::register_routes(crow::SimpleApp& app) {
  // GET /api/households/<int>/statistics/overview：家庭总览，查询参数
  // year / month 缺省为当前年月。
  CROW_ROUTE(app, "/api/households/<int>/statistics/overview").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const int year = http::query_int(request, "year", current_year());
          const int month = http::query_int(request, "month", current_month());
          return dto::to_json(service_.overview(id, year, month));
        });
      });

  // GET /api/households/<int>/statistics/period：区间统计；from/to 必填，
  // owner_member_id 可选用于按成员过滤。
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

  // GET /api/households/<int>/statistics/monthly：近 N 个月收支趋势，
  // months 缺省 6（1..36），返回按月升序、缺月补零的数组。
  CROW_ROUTE(app, "/api/households/<int>/statistics/monthly").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const int months = http::query_int(request, "months", 6);
          nlohmann::json data = nlohmann::json::array();
          for (const auto& item : service_.monthly(id, months)) {
            data.push_back(dto::to_json(item));
          }
          return data;
        });
      });
}

}  // namespace wt

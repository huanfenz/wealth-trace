// 家庭控制器实现：解析请求 -> 调用 HouseholdService -> 序列化 JSON。
#include "controller/household_controller.hpp"

#include <cstdint>

#include <nlohmann/json.hpp>

#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"

namespace wt {

void HouseholdController::register_routes(crow::SimpleApp& app) {
  // GET /api/households：列出全部家庭。
  CROW_ROUTE(app, "/api/households").methods("GET"_method)([this] {
    return http::handle([this] {
      nlohmann::json data = nlohmann::json::array();
      for (const auto& household : service_.list()) {
        data.push_back(dto::to_json(household));
      }
      return data;
    });
  });

  // POST /api/households：新建家庭，请求体可选 name，缺省用「我的家庭」。
  CROW_ROUTE(app, "/api/households").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto name = dto::optional_string_or(body, "name", "我的家庭");
          return dto::to_json(service_.create(name));
        });
      });

  // GET /api/households/<int>：按 id 查询家庭。
  CROW_ROUTE(app, "/api/households/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get(id)); });
  });

  // PUT /api/households/<int>：更新家庭名称；请求体 name 为空或未提供时沿用原名。
  CROW_ROUTE(app, "/api/households/<int>").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto name = dto::optional_string_or(body, "name", "");
          const auto existing = service_.get(id);
          const std::string resolved_name =
              name.empty() ? existing.name : name;
          return dto::to_json(service_.update(id, resolved_name));
        });
      });
}

}  // namespace wt

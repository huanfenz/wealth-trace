#include "controller/household_controller.hpp"

#include <cstdint>

#include <nlohmann/json.hpp>

#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"

namespace wt {

void HouseholdController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/households").methods("GET"_method)([this] {
    return http::handle([this] {
      nlohmann::json data = nlohmann::json::array();
      for (const auto& household : service_.list()) {
        data.push_back(dto::to_json(household));
      }
      return data;
    });
  });

  CROW_ROUTE(app, "/api/households").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto name = dto::optional_string_or(body, "name", "我的家庭");
          return dto::to_json(service_.create(name));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get(id)); });
  });

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

#include "controller/category_controller.hpp"

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "dto/json_helpers.hpp"
#include "model/enums.hpp"

namespace wt {
namespace {
nlohmann::json category_json(const TransactionCategory& c) {
  return {{"id", c.id}, {"household_id", c.household_id},
          {"type", std::string(to_string(c.type))}, {"name", c.name},
          {"sort_order", c.sort_order}, {"active", c.active},
          {"created_at", c.created_at}, {"updated_at", c.updated_at}};
}
TransactionType category_type(const std::string& value) {
  const auto type = parse_transaction_type(value);
  if (!type || (*type != TransactionType::Income && *type != TransactionType::Expense))
    throw invalid_request("type must be INCOME or EXPENSE");
  return *type;
}
}

void CategoryController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/households/<int>/categories").methods("GET"_method)(
      [this](const crow::request& req, int household_id) {
        return http::handle([this, &req, household_id] {
          const auto type = category_type(http::query_string(req, "type").value_or(""));
          const bool all = http::query_string(req, "include_inactive").value_or("false") == "true";
          nlohmann::json result = nlohmann::json::array();
          for (const auto& c : service_.list(household_id, type, all)) result.push_back(category_json(c));
          return result;
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/categories").methods("POST"_method)(
      [this](const crow::request& req, int household_id) {
        return http::handle([this, &req, household_id] {
          const auto body = dto::parse_object(req.body);
          const auto type = category_type(dto::require_string(body, "type", 16));
          const auto name = dto::require_string(body, "name", 64);
          return category_json(service_.create(household_id, type, name));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/categories/<int>").methods("PUT"_method)(
      [this](const crow::request& req, int household_id, int id) {
        return http::handle([this, &req, household_id, id] {
          const auto body = dto::parse_object(req.body);
          const auto name = dto::require_string(body, "name", 64);
          const int sort_order = body.contains("sort_order") && body["sort_order"].is_number_integer()
                                     ? body["sort_order"].get<int>() : 0;
          return category_json(service_.update(household_id, id, name, sort_order));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/categories/<int>/status").methods("PUT"_method)(
      [this](const crow::request& req, int household_id, int id) {
        return http::handle([this, &req, household_id, id] {
          const auto body = dto::parse_object(req.body);
          if (!body.contains("active") || !body["active"].is_boolean())
            throw invalid_request("active must be a boolean");
          return category_json(service_.set_active(household_id, id, body["active"].get<bool>()));
        });
      });
}
}  // namespace wt

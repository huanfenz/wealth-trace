#include "controller/transaction_controller.hpp"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"
#include "model/enums.hpp"

namespace wt {
namespace {

std::optional<std::string> body_time(const nlohmann::json& body) {
  return dto::optional_string(body, "transaction_time", 19);
}

std::optional<std::string> body_remark(const nlohmann::json& body) {
  return dto::optional_string(body, "remark", 500);
}

std::optional<std::string> body_category(const nlohmann::json& body) {
  return dto::optional_string(body, "category", 64);
}

}  // namespace

void TransactionController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/households/<int>/transactions").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          TransactionQuery query;
          query.household_id = id;
          query.owner_member_id = http::query_int64(request, "owner_member_id");
          query.asset_id = http::query_int64(request, "asset_id");
          if (const auto type = http::query_string(request, "type"); type.has_value()) {
            const auto parsed = parse_transaction_type(*type);
            if (!parsed.has_value()) {
              throw invalid_request("invalid transaction type");
            }
            query.type = *parsed;
          }
          query.from_time = http::query_string(request, "from");
          query.to_time = http::query_string(request, "to");
          query.limit = http::query_int(request, "limit", 200);
          if (query.limit <= 0 || query.limit > 1000) {
            throw invalid_request("limit must be between 1 and 1000");
          }
          query.offset = http::query_int(request, "offset", 0);

          nlohmann::json items = nlohmann::json::array();
          for (const auto& transaction : service_.list(query)) {
            items.push_back(dto::to_json(transaction));
          }
          return nlohmann::json{{"total", service_.count(query)}, {"items", items}};
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/transactions/income").methods("POST"_method)(
      [this](const crow::request& request, int /*id*/) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto asset_id = dto::require_int64(body, "asset_id");
          const auto amount = dto::require_int64(body, "amount");
          return dto::to_json(service_.record_income(
              asset_id, body_category(body), amount, body_time(body).value_or(""),
              body_remark(body)));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/transactions/expense").methods("POST"_method)(
      [this](const crow::request& request, int /*id*/) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto asset_id = dto::require_int64(body, "asset_id");
          const auto amount = dto::require_int64(body, "amount");
          return dto::to_json(service_.record_expense(
              asset_id, body_category(body), amount, body_time(body).value_or(""),
              body_remark(body)));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/transactions/adjustment")
      .methods("POST"_method)([this](const crow::request& request, int /*id*/) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto asset_id = dto::require_int64(body, "asset_id");
          const auto amount = dto::require_int64(body, "amount");
          return dto::to_json(service_.record_adjustment(
              asset_id, amount, body_time(body).value_or(""), body_remark(body)));
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/transfers").methods("POST"_method)(
      [this](const crow::request& request, int /*id*/) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto from_asset_id = dto::require_int64(body, "from_asset_id");
          const auto to_asset_id = dto::require_int64(body, "to_asset_id");
          const auto amount = dto::require_int64(body, "amount");
          return dto::to_json(service_.transfer(from_asset_id, to_asset_id, amount,
                                                body_time(body).value_or(""),
                                                body_remark(body)));
        });
      });

  CROW_ROUTE(app, "/api/transactions/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get(id)); });
  });
}

}  // namespace wt

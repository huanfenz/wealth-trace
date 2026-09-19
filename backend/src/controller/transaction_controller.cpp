// 交易控制器实现：解析请求 -> 调用 TransactionService -> 序列化 JSON。
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

// 读取可选交易时间（UTC "YYYY-MM-DD HH:MM:SS"，长度 19）。
std::optional<std::string> body_time(const nlohmann::json& body) {
  return dto::optional_string(body, "transaction_time", 19);
}

// 读取可选备注。
std::optional<std::string> body_remark(const nlohmann::json& body) {
  return dto::optional_string(body, "remark", 500);
}

// 读取可选收支分类。
std::optional<std::string> body_category(const nlohmann::json& body) {
  return dto::optional_string(body, "category", 64);
}

}  // namespace

void TransactionController::register_routes(crow::SimpleApp& app) {
  // GET /api/households/<int>/transactions：分页查询流水，支持查询参数
  // owner_member_id、asset_id、type、from、to、limit（默认 200，1~1000）、offset，
  // 返回 {"total","items"}。
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

  // POST /api/households/<int>/transactions/income：记一笔收入；
  // asset_id、amount（分）必填，category/transaction_time/remark 可选。
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

  // POST /api/households/<int>/transactions/expense：记一笔支出；
  // asset_id、amount（分）必填，category/transaction_time/remark 可选。
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

  // POST /api/households/<int>/transactions/adjustment：对资产做余额调整；
  // asset_id、amount（分）必填。
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

  // POST /api/households/<int>/transfers：资产间转账，生成一对转出/转入流水；
  // from_asset_id、to_asset_id、amount（分）必填。
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

  // GET /api/transactions/<int>：按 id 查询单条流水。
  CROW_ROUTE(app, "/api/transactions/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get(id)); });
  });
}

}  // namespace wt

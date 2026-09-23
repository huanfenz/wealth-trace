// 账户控制器实现：解析请求 -> 调用 AccountService -> 序列化 JSON。
#include "controller/account_controller.hpp"

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

// 解析可选 type 字段：未提供用 fallback；提供但取值非法则报参数错误。
AccountType resolve_account_type(const nlohmann::json& body, AccountType fallback) {
  const auto value = dto::optional_string(body, "type", 32);
  if (!value.has_value()) {
    return fallback;
  }
  const auto parsed = parse_account_type(*value);
  if (!parsed.has_value()) {
    throw invalid_request("invalid account type");
  }
  return *parsed;
}

}  // namespace

void AccountController::register_routes(crow::SimpleApp& app) {
  // GET /api/households/<int>/accounts：列出某家庭账户（带余额与资产数），
  // 支持查询参数 owner_member_id 过滤。
  CROW_ROUTE(app, "/api/households/<int>/accounts").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto owner = http::query_int64(request, "owner_member_id");
          nlohmann::json data = nlohmann::json::array();
          for (const auto& view : service_.list_views(id, owner)) {
            data.push_back(dto::to_json(view));
          }
          return data;
        });
      });

  // POST /api/households/<int>/accounts：新建账户；owner_member_id 与 name
  // 必填，type 缺省 BANK，机构/掩码卡号/备注可选，enabled 缺省 true。
  CROW_ROUTE(app, "/api/households/<int>/accounts").methods("POST"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto owner_member_id = dto::require_int64(body, "owner_member_id");
          const auto name = dto::require_string(body, "name", 100);
          const auto type = resolve_account_type(body, AccountType::Bank);
          const auto institution = dto::optional_string(body, "institution_name", 100);
          const auto masked = dto::optional_string(body, "account_no_masked", 64);
          const auto remark = dto::optional_string(body, "remark", 500);
          const bool enabled = dto::optional_bool(body, "enabled", true);
          return dto::to_json(service_.create(id, owner_member_id, name, type, institution,
                                              masked, remark, enabled));
        });
      });

  // GET /api/accounts/<int>：按 id 查询账户视图（含余额与资产数）。
  CROW_ROUTE(app, "/api/accounts/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get_view(id)); });
  });

  // PUT /api/accounts/<int>：更新账户；未提供的字段沿用原值，其中
  // 机构/掩码卡号/备注显式提供 null 可清空。
  CROW_ROUTE(app, "/api/accounts/<int>").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto existing = service_.get(id);
          const auto owner = dto::optional_int64(body, "owner_member_id")
                                 .value_or(existing.owner_member_id);
          const auto name = dto::optional_string_or(body, "name", existing.name);
          const auto type = resolve_account_type(body, existing.type);
          const auto institution = body.contains("institution_name")
                                       ? dto::optional_string(body, "institution_name", 100)
                                       : existing.institution_name;
          const auto masked = body.contains("account_no_masked")
                                  ? dto::optional_string(body, "account_no_masked", 64)
                                  : existing.account_no_masked;
          const auto remark = body.contains("remark")
                                  ? dto::optional_string(body, "remark", 500)
                                  : existing.remark;
          const bool enabled = dto::optional_bool(body, "enabled", existing.enabled);
          return dto::to_json(service_.update(id, owner, name, type, institution, masked,
                                              remark, enabled));
        });
      });

  // DELETE /api/accounts/<int>：删除账户；账户不存在抛 404，账户下仍有资产时
  // 抛 409（需先清空资产），成功返回 null。
  CROW_ROUTE(app, "/api/accounts/<int>").methods("DELETE"_method)([this](int id) {
    return http::handle([this, id] {
      service_.remove(id);
      return nlohmann::json(nullptr);
    });
  });
}

}  // namespace wt

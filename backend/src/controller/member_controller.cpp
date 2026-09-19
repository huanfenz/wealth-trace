// 成员控制器实现：解析请求 -> 调用 MemberService -> 序列化 JSON。
#include "controller/member_controller.hpp"

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

// 解析可选 role 字段：未提供用 fallback；提供但取值非法则报参数错误。
MemberRole resolve_role(const nlohmann::json& body, MemberRole fallback) {
  const auto value = dto::optional_string(body, "role", 32);
  if (!value.has_value()) {
    return fallback;
  }
  const auto parsed = parse_member_role(*value);
  if (!parsed.has_value()) {
    throw invalid_request("role must be OWNER or MEMBER");
  }
  return *parsed;
}

// 解析可选 status 字段：未提供用 fallback；提供但取值非法则报参数错误。
MemberStatus resolve_status(const nlohmann::json& body, MemberStatus fallback) {
  const auto value = dto::optional_string(body, "status", 32);
  if (!value.has_value()) {
    return fallback;
  }
  const auto parsed = parse_member_status(*value);
  if (!parsed.has_value()) {
    throw invalid_request("status must be ACTIVE or INACTIVE");
  }
  return *parsed;
}

}  // namespace

void MemberController::register_routes(crow::SimpleApp& app) {
  // GET /api/households/<int>/members：列出某家庭的全部成员。
  CROW_ROUTE(app, "/api/households/<int>/members").methods("GET"_method)([this](int id) {
    return http::handle([this, id] {
      nlohmann::json data = nlohmann::json::array();
      for (const auto& member : service_.list(id)) {
        data.push_back(dto::to_json(member));
      }
      return data;
    });
  });

  // POST /api/households/<int>/members：新建成员；name 必填，role/status 可选。
  CROW_ROUTE(app, "/api/households/<int>/members").methods("POST"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto name = dto::require_string(body, "name", 100);
          const auto role = resolve_role(body, MemberRole::Member);
          const auto status = resolve_status(body, MemberStatus::Active);
          return dto::to_json(service_.create(id, name, role, status));
        });
      });

  // GET /api/members/<int>：按 id 查询成员。
  CROW_ROUTE(app, "/api/members/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get(id)); });
  });

  // PUT /api/members/<int>：更新成员；未提供的字段沿用原值。
  CROW_ROUTE(app, "/api/members/<int>").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto existing = service_.get(id);
          const auto name = dto::optional_string_or(body, "name", existing.name);
          const auto role = resolve_role(body, existing.role);
          const auto status = resolve_status(body, existing.status);
          return dto::to_json(service_.update(id, name, role, status));
        });
      });
}

}  // namespace wt

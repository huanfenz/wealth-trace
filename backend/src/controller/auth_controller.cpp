// 鉴权控制器实现：/api/auth 下的登录、建号、登出与改密。
#include "controller/auth_controller.hpp"

#include <string>

#include <nlohmann/json.hpp>

#include "controller/http_util.hpp"
#include "dto/json_helpers.hpp"

namespace wt {
namespace {

nlohmann::json session_json(const AuthSessionInfo& session) {
  return {{"token", session.token},
          {"user", {{"id", session.user_id}, {"username", session.username}}},
          {"expires_at", session.expires_at}};
}

// 从 Authorization 头中取出 Bearer 令牌原文（路由已过中间件，必为有效令牌）。
std::string bearer_token(const crow::request& request) {
  static constexpr const char kPrefix[] = "Bearer ";
  const std::string& authorization = request.get_header_value("Authorization");
  if (authorization.compare(0, sizeof(kPrefix) - 1, kPrefix) != 0) {
    return {};
  }
  return authorization.substr(sizeof(kPrefix) - 1);
}

}  // namespace

void AuthController::register_routes(App& app) {
  CROW_ROUTE(app, "/api/auth/status").methods("GET"_method)([this] {
    return http::handle([this] {
      return nlohmann::json{{"enabled", service_.enabled()},
                            {"initialized", service_.initialized()}};
    });
  });

  CROW_ROUTE(app, "/api/auth/setup").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto username = dto::require_string(body, "username", 64);
          const auto password = dto::require_string(body, "password", 128);
          return session_json(service_.setup(username, password));
        });
      });

  CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto username = dto::require_string(body, "username", 64);
          const auto password = dto::require_string(body, "password", 128);
          return session_json(service_.login(username, password));
        });
      });

  CROW_ROUTE(app, "/api/auth/logout").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          service_.logout(bearer_token(request));
          return nlohmann::json{{"logged_out", true}};
        });
      });

  CROW_ROUTE(app, "/api/auth/password").methods("POST"_method)(
      [this](const crow::request& request) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          const auto old_password = dto::require_string(body, "old_password", 128);
          const auto new_password = dto::require_string(body, "new_password", 128);
          service_.change_password(bearer_token(request), old_password, new_password);
          return nlohmann::json{{"changed", true}};
        });
      });
}

}  // namespace wt

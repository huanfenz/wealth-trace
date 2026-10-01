// 全局鉴权中间件：拦截 /api/* 请求并校验 Authorization: Bearer <token>。
// 放行：静态资源（非 /api/ 前缀）、/api/health、三个公开鉴权端点、鉴权关闭模式。
// 校验失败时 res.end() 短路：后续中间件与路由处理器均不会执行。
#pragma once

#include <string>

#include "crow.h"

#include "common/error.hpp"
#include "common/response.hpp"
#include "service/auth_service.hpp"

namespace wt {

struct AuthMiddleware {
  struct context {};

  // Crow 要求中间件可默认构造（App 构造时创建），服务引用随后经 configure 注入。
  AuthMiddleware() = default;

  void before_handle(crow::request& req, crow::response& res, context&) {
    if (!enabled_ || service_ == nullptr) {
      return;  // 鉴权关闭（仅限本地开发；非回环绑定已在启动时拒绝）。
    }
    const std::string& url = req.url;
    if (url.rfind("/api/", 0) != 0) {
      return;  // 静态资源与根路径：SPA 壳必须免鉴权，否则登录页无法加载。
    }
    if (url == "/api/health") {
      return;  // 部署与监控依赖匿名健康检查。
    }
    if (url == "/api/auth/status" || url == "/api/auth/setup" || url == "/api/auth/login") {
      return;  // 鉴权自身的公开端点。
    }
    static constexpr const char kBearerPrefix[] = "Bearer ";
    const std::string& authorization = req.get_header_value("Authorization");
    if (authorization.compare(0, sizeof(kBearerPrefix) - 1, kBearerPrefix) == 0 &&
        service_->validate(authorization.substr(sizeof(kBearerPrefix) - 1))) {
      return;
    }
    res.code = 401;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.body = error_body(error_code::kUnauthorized, "请先登录");
    res.end();
  }

  void after_handle(crow::request&, crow::response&, context&) {}

  void configure(AuthService& service, bool enabled) {
    service_ = &service;
    enabled_ = enabled;
  }

 private:
  AuthService* service_ = nullptr;
  bool enabled_ = false;
};

// 全项目使用的应用类型：带上鉴权中间件的 Crow 实例。
using App = crow::App<AuthMiddleware>;

}  // namespace wt

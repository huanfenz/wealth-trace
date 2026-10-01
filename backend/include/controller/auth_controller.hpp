#pragma once
#include "controller/auth_middleware.hpp"
#include "service/auth_service.hpp"

namespace wt {

// 鉴权控制器：登录/建号/登出/改密。status、setup、login 为公开端点（中间件放行）。
class AuthController {
 public:
  explicit AuthController(AuthService& service) : service_(service) {}
  void register_routes(App& app);

 private:
  AuthService& service_;
};

}  // namespace wt

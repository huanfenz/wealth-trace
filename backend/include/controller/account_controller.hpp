#pragma once

// 账户控制器声明：只负责解析请求、调用 AccountService、序列化响应。
#include "crow.h"

#include "service/account_service.hpp"

namespace wt {

class Database;

// 账户资源的 REST 接口：家庭下账户列表 / 新建 / 查询 / 更新 / 删除。
class AccountController {
 public:
  explicit AccountController(Database& database) : service_(database) {}

  // 注册 /api/households/<int>/accounts 与 /api/accounts/<int> 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  AccountService service_;
};

}  // namespace wt

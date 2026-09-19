#pragma once

// 家庭控制器声明：只负责解析请求、调用 HouseholdService、序列化响应。
#include "crow.h"

#include "service/household_service.hpp"

namespace wt {

class Database;

// 家庭资源的 REST 接口：列表 / 新建 / 查询 / 更新。
class HouseholdController {
 public:
  explicit HouseholdController(Database& database) : service_(database) {}

  // 注册 /api/households 相关路由。
  void register_routes(crow::SimpleApp& app);

 private:
  HouseholdService service_;
};

}  // namespace wt

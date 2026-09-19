#pragma once

// 统计控制器声明：只负责解析请求、调用 StatisticsService、序列化响应。
#include "crow.h"

#include "service/statistics_service.hpp"

namespace wt {

class Database;

// 统计接口：家庭总览（按年 / 月）与区间统计（按成员可选）。
class StatisticsController {
 public:
  explicit StatisticsController(Database& database) : service_(database) {}

  // 注册 /api/households/<int>/statistics/... 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  StatisticsService service_;
};

}  // namespace wt

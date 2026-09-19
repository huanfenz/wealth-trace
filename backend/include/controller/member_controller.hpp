#pragma once

// 家庭成员控制器声明：只负责解析请求、调用 MemberService、序列化响应。
#include "crow.h"

#include "service/member_service.hpp"

namespace wt {

class Database;

// 成员资源的 REST 接口：家庭下成员列表 / 新建 / 查询 / 更新。
class MemberController {
 public:
  explicit MemberController(Database& database) : service_(database) {}

  // 注册 /api/households/<int>/members 与 /api/members/<int> 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  MemberService service_;
};

}  // namespace wt

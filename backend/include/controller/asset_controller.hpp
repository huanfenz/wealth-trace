#pragma once

// 资产控制器声明：只负责解析请求、调用 AssetService、序列化响应。
#include "crow.h"

#include "service/asset_service.hpp"

namespace wt {

class Database;

// 资产资源的 REST 接口：列表 / 新建（含明细块）/ 查询 / 改元数据 /
// 改状态 / 改明细。
class AssetController {
 public:
  explicit AssetController(Database& database) : service_(database) {}

  // 注册 /api/households/<int>/assets 与 /api/assets/<int>... 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  AssetService service_;
};

}  // namespace wt

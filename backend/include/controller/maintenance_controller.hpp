#pragma once

// 维护控制器声明：手动预览 / 触发每日资产维护，只做请求处理与序列化。
#include "crow.h"

#include "service/daily_maintenance_service.hpp"

namespace wt {

class Database;

// 维护资源的 REST 接口：预览将产生的变更、强制执行一次维护（幂等）。
class MaintenanceController {
 public:
  explicit MaintenanceController(Database& database) : service_(database) {}

  // 注册 /api/maintenance/preview 与 /api/maintenance/run 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  DailyAssetMaintenanceService service_;
};

}  // namespace wt

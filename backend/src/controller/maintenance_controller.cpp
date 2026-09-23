// 维护控制器实现：手动预览 / 触发每日资产维护。
#include "controller/maintenance_controller.hpp"

#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/serialization.hpp"

namespace wt {

void MaintenanceController::register_routes(crow::SimpleApp& app) {
  // GET /api/maintenance/preview：预览按当前业务日期执行维护将产生的变更，不落库。
  CROW_ROUTE(app, "/api/maintenance/preview").methods("GET"_method)([this] {
    return http::handle([this] { return dto::to_json(service_.preview()); });
  });

  // POST /api/maintenance/run：强制执行一次每日维护（幂等），返回推进条数。
  CROW_ROUTE(app, "/api/maintenance/run").methods("POST"_method)([this] {
    return http::handle([this] { return dto::to_json(service_.run()); });
  });
}

}  // namespace wt

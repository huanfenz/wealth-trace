// 后端入口：加载配置 -> 打开数据库 -> 执行 migration -> 创建默认家庭 ->
// 注册 API 路由与 CORS 预检 -> 注册静态托管 -> 启动 Crow 服务。
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "crow.h"

#include "common/logging.hpp"
#include "common/response.hpp"
#include "config/config.hpp"
#include "controller/account_controller.hpp"
#include "controller/asset_controller.hpp"
#include "controller/household_controller.hpp"
#include "controller/http_util.hpp"
#include "controller/member_controller.hpp"
#include "controller/meta_controller.hpp"
#include "controller/statistics_controller.hpp"
#include "controller/static_file_controller.hpp"
#include "controller/transaction_controller.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "service/household_service.hpp"

namespace {

// 确定配置文件路径：优先命令行第一个参数，其次环境变量 WEALTH_TRACE_CONFIG，
// 最后回退到当前目录的 config.json。
std::string config_path(int argc, char** argv) {
  if (argc > 1) {
    return argv[1];
  }
  if (const char* env = std::getenv("WEALTH_TRACE_CONFIG"); env != nullptr) {
    return env;
  }
  return "config.json";
}

}  // namespace

int main(int argc, char** argv) {
  using namespace wt;

  // 1. 加载配置；失败直接退出进程。
  Config config;
  try {
    config = Config::load(config_path(argc, argv));
  } catch (const std::exception& error) {
    log_critical(std::string("configuration error: ") + error.what());
    return 1;
  }
  config.apply_log_level();

  log_info("财迹 wealth-trace backend starting");

  // 2. 打开数据库、执行 migration 并确保存在一个默认家庭。
  Database database;
  try {
    database.open(config.database.path, config.database.busy_timeout_ms,
                  config.database.wal);
    MigrationRunner runner(database);
    const auto applied = runner.run(config.database.migrations_dir);
    log_info("database ready: " + config.database.path + " (schema version " +
             std::to_string(runner.current_version()) + ", applied " +
             std::to_string(applied.size()) + " migration(s))");

    HouseholdService households(database);
    const auto household = households.ensure_default("我的家庭");
    log_info("active household: #" + std::to_string(household.id) + " " + household.name);
  } catch (const std::exception& error) {
    log_critical(std::string("database initialisation failed: ") + error.what());
    return 1;
  }

  // 3. 创建 Crow 应用并注册路由。
  crow::SimpleApp app;
  app.loglevel(crow::LogLevel::Warning);

  // 健康检查路由。
  CROW_ROUTE(app, "/api/health").methods("GET"_method)([] {
    return http::ok(nlohmann::json{{"status", "ok"}});
  });

  // 构造各控制器（每个控制器内部持有对应 Service）。
  HouseholdController household_controller(database);
  MemberController member_controller(database);
  AccountController account_controller(database);
  AssetController asset_controller(database);
  TransactionController transaction_controller(database);
  StatisticsController statistics_controller(database);
  MetaController meta_controller(config.categories);
  StaticFileController static_controller(config.frontend);

  // 先注册所有 API 路由，确保其优先于后面的静态文件通配路由。
  household_controller.register_routes(app);
  member_controller.register_routes(app);
  account_controller.register_routes(app);
  asset_controller.register_routes(app);
  transaction_controller.register_routes(app);
  statistics_controller.register_routes(app);
  meta_controller.register_routes(app);
  // 任意 /api/ 路径的 CORS OPTIONS 预检请求统一返回 204。
  CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)(
      [](const crow::request&, std::string) {
        crow::response response(204);
        response.set_header("Access-Control-Allow-Origin", "*");
        response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        response.set_header("Access-Control-Allow-Methods",
                            "GET, POST, PUT, DELETE, OPTIONS");
        return response;
      });

  // 最后注册静态托管：API 路由已先注册，因此前者始终优先。
  static_controller.register_routes(app);

  // 4. 绑定地址端口，设置线程数并启动服务（阻塞运行）。
  app.bindaddr(config.server.host).port(static_cast<std::uint16_t>(config.server.port));

  log_info("listening on http://" + config.server.host + ":" +
           std::to_string(config.server.port));
  app.concurrency(static_cast<std::uint16_t>(config.server.threads)).run();

  log_info("backend stopped");
  return 0;
}

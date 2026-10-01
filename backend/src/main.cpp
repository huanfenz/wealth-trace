// 后端入口：加载配置 -> 打开数据库 -> 执行 migration -> 创建默认家庭 ->
// 注册 API 路由与 CORS 预检 -> 注册静态托管 -> 启动 Crow 服务。
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "crow.h"

#include "common/logging.hpp"
#include "common/response.hpp"
#include "config/config.hpp"
#include "controller/account_controller.hpp"
#include "controller/asset_controller.hpp"
#include "controller/auth_controller.hpp"
#include "controller/auth_middleware.hpp"
#include "controller/category_controller.hpp"
#include "controller/household_controller.hpp"
#include "controller/http_util.hpp"
#include "controller/maintenance_controller.hpp"
#include "controller/database_controller.hpp"
#include "controller/recurring_investment_controller.hpp"
#include "controller/member_controller.hpp"
#include "controller/meta_controller.hpp"
#include "controller/statistics_controller.hpp"
#include "controller/static_file_controller.hpp"
#include "controller/transaction_controller.hpp"
#include "database/database.hpp"
#include "database/migration.hpp"
#include "service/daily_maintenance_service.hpp"
#include "service/recurring_investment_service.hpp"
#include "service/household_service.hpp"
#include "utils/time_util.hpp"

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

// 是否为回环地址（本机开发场景，允许关闭鉴权）。
bool is_loopback(const std::string& host) {
  return host == "127.0.0.1" || host == "::1" || host == "localhost";
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
  // 统一业务时区（默认 Asia/Shanghai）：业务日期相关判断全部以它为准，
  // 必须在创建线程、处理请求之前设置。审计时间戳仍为 UTC。
  time_util::set_business_timezone(config.business_timezone);

  log_info("财迹 wealth-trace backend starting (business timezone: " +
           time_util::business_timezone() + ")");

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

  // 2.1 每日维护补跑：程序不保证每天 0 点在运行，启动时检查当天是否已执行，
  //     未执行则立即补跑一次。失败不阻断启动，仅记录错误，下次调度可重试。
  try {
    DailyAssetMaintenanceService maintenance(database);
    if (maintenance.run_if_due()) {
      log_info("daily maintenance catch-up executed on startup");
    }
  } catch (const std::exception& error) {
    log_error(std::string("daily maintenance catch-up failed: ") + error.what());
  }
  try {
    RecurringInvestmentService investments(database);
    investments.process_due();
  } catch (const std::exception& error) {
    log_error(std::string("recurring investment catch-up failed: ") + error.what());
  }

  // 2.2 鉴权模式硬闸：关闭鉴权只允许搭配回环地址绑定（本地开发）。
  //     显式确认 allow_unauthenticated_lan 才可例外，且打印醒目警告。
  if (!config.auth.required() && !is_loopback(config.server.host)) {
    if (!config.auth.allow_unauthenticated_lan) {
      log_critical(
          "auth.mode=disabled only allowed on loopback; set auth.mode=required or "
          "explicitly set auth.allow_unauthenticated_lan=true to override");
      return 1;
    }
    log_warn("!!! auth disabled on non-loopback address per explicit config override !!!");
  }

  // 3. 创建 Crow 应用并注册路由。App 携带全局鉴权中间件，
  //    所有 /api/* 请求（公开端点除外）都需携带有效的 Bearer 会话令牌。
  App app;
  app.loglevel(crow::LogLevel::Warning);

  AuthService auth_service(database, config.auth.required());
  app.get_middleware<AuthMiddleware>().configure(auth_service, config.auth.required());

  // 健康检查路由（中间件放行，供部署/监控匿名探测）。
  CROW_ROUTE(app, "/api/health").methods("GET"_method)([] {
    return http::ok(nlohmann::json{{"status", "ok"}});
  });

  // 构造各控制器（每个控制器内部持有对应 Service）。
  HouseholdController household_controller(database);
  MemberController member_controller(database);
  AccountController account_controller(database);
  AssetController asset_controller(database);
  TransactionController transaction_controller(database);
  CategoryController category_controller(database, config.categories);
  StatisticsController statistics_controller(database);
  MetaController meta_controller(config.categories);
  StaticFileController static_controller(config.frontend);
  MaintenanceController maintenance_controller(database);
  RecurringInvestmentController investment_controller(database);
  DatabaseController database_controller(database, config.database.path,
                                         config.database.migrations_dir);
  AuthController auth_controller(auth_service);

  // 先注册所有 API 路由，确保其优先于后面的静态文件通配路由。
  household_controller.register_routes(app);
  member_controller.register_routes(app);
  account_controller.register_routes(app);
  asset_controller.register_routes(app);
  transaction_controller.register_routes(app);
  category_controller.register_routes(app);
  statistics_controller.register_routes(app);
  meta_controller.register_routes(app);
  maintenance_controller.register_routes(app);
  investment_controller.register_routes(app);
  database_controller.register_routes(app);
  auth_controller.register_routes(app);

  // 最后注册静态托管：API 路由已先注册，因此前者始终优先。
  static_controller.register_routes(app);

  // 4. 绑定地址端口，设置线程数并启动服务（阻塞运行）。
  app.bindaddr(config.server.host).port(static_cast<std::uint16_t>(config.server.port));

  // 4.1 每日维护调度：后台线程等到下一个「业务时区 0 点」执行一次，循环往复。
  //     只做「收敛到今天应有的状态」，不逐日回放；run_if_due 保证幂等与去重。
  //     线程可停止、在退出前 join：detach 版本会在 main 返回后继续使用已被
  //     析构的 database（栈对象），关停时刻构成未定义行为。
  std::mutex stop_mutex;
  std::condition_variable stop_signal;
  bool stopping = false;
  std::thread maintenance_scheduler([&database, &stop_mutex, &stop_signal, &stopping] {
    std::unique_lock<std::mutex> lock(stop_mutex);
    while (!stopping) {
      // 最长等到下一个业务 0 点；期间被停止信号唤醒则立即退出。
      const auto wait = std::chrono::seconds(time_util::seconds_until_next_business_midnight());
      if (stop_signal.wait_for(lock, wait, [&stopping] { return stopping; })) {
        break;
      }
      lock.unlock();
      try {
        DailyAssetMaintenanceService maintenance(database);
        maintenance.run_if_due();
      } catch (const std::exception& error) {
        log_error(std::string("daily maintenance failed: ") + error.what());
      }
      try {
        RecurringInvestmentService investments(database);
        investments.process_due();
      } catch (const std::exception& error) {
        log_error(std::string("recurring investment run failed: ") + error.what());
      }
      lock.lock();
    }
  });

  log_info("listening on http://" + config.server.host + ":" +
           std::to_string(config.server.port));
  app.concurrency(static_cast<std::uint16_t>(config.server.threads)).run();

  // 服务停止后：先停掉维护线程并 join，再依次析构 database 等栈对象。
  {
    std::scoped_lock<std::mutex> lock(stop_mutex);
    stopping = true;
  }
  stop_signal.notify_all();
  if (maintenance_scheduler.joinable()) {
    maintenance_scheduler.join();
  }

  log_info("backend stopped");
  return 0;
}

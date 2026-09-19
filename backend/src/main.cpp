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

  Config config;
  try {
    config = Config::load(config_path(argc, argv));
  } catch (const std::exception& error) {
    log_critical(std::string("configuration error: ") + error.what());
    return 1;
  }
  config.apply_log_level();

  log_info("财迹 wealth-trace backend starting");

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

  crow::SimpleApp app;
  app.loglevel(crow::LogLevel::Warning);

  CROW_ROUTE(app, "/api/health").methods("GET"_method)([] {
    return http::ok(nlohmann::json{{"status", "ok"}});
  });

  HouseholdController household_controller(database);
  MemberController member_controller(database);
  AccountController account_controller(database);
  AssetController asset_controller(database);
  TransactionController transaction_controller(database);
  StatisticsController statistics_controller(database);
  MetaController meta_controller(config.categories);
  StaticFileController static_controller(config.frontend);

  household_controller.register_routes(app);
  member_controller.register_routes(app);
  account_controller.register_routes(app);
  asset_controller.register_routes(app);
  transaction_controller.register_routes(app);
  statistics_controller.register_routes(app);
  meta_controller.register_routes(app);
  // CORS preflight for any API path.
  CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)(
      [](const crow::request&, std::string) {
        crow::response response(204);
        response.set_header("Access-Control-Allow-Origin", "*");
        response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        response.set_header("Access-Control-Allow-Methods",
                            "GET, POST, PUT, DELETE, OPTIONS");
        return response;
      });

  // Serve the built frontend last so that API routes always take precedence.
  static_controller.register_routes(app);

  app.bindaddr(config.server.host).port(static_cast<std::uint16_t>(config.server.port));

  log_info("listening on http://" + config.server.host + ":" +
           std::to_string(config.server.port));
  app.concurrency(static_cast<std::uint16_t>(config.server.threads)).run();

  log_info("backend stopped");
  return 0;
}

#include "config/config.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "common/logging.hpp"

namespace wt {
namespace {

using nlohmann::json;

int json_int(const json& object, const char* key, int fallback) {
  if (object.contains(key) && object.at(key).is_number_integer()) {
    return object.at(key).get<int>();
  }
  return fallback;
}

std::string json_string(const json& object, const char* key, const std::string& fallback) {
  if (object.contains(key) && object.at(key).is_string()) {
    return object.at(key).get<std::string>();
  }
  return fallback;
}

bool json_bool(const json& object, const char* key, bool fallback) {
  if (object.contains(key) && object.at(key).is_boolean()) {
    return object.at(key).get<bool>();
  }
  return fallback;
}

std::vector<std::string> json_string_list(const json& object, const char* key,
                                          std::vector<std::string> fallback) {
  if (!object.contains(key) || !object.at(key).is_array()) {
    return fallback;
  }
  std::vector<std::string> values;
  for (const auto& item : object.at(key)) {
    if (item.is_string()) {
      values.push_back(item.get<std::string>());
    }
  }
  return values;
}

}  // namespace

Config Config::load(const std::string& path) {
  Config config;
  config.categories.expense = {"餐饮", "交通", "购物", "娱乐", "居住",
                               "医疗", "教育", "通讯", "人情", "其他"};
  config.categories.income = {"工资", "奖金", "红包", "利息收入",
                              "投资收益", "退款", "其他"};

  std::ifstream input(path);
  if (!input.is_open()) {
    log_warn("config file not found, using defaults: " + path);
    return config;
  }

  json root;
  try {
    input >> root;
  } catch (const json::exception& error) {
    throw std::runtime_error("failed to parse config file '" + path + "': " + error.what());
  }
  if (!root.is_object()) {
    throw std::runtime_error("config file root must be a JSON object: " + path);
  }

  if (root.contains("server") && root.at("server").is_object()) {
    const auto& server = root.at("server");
    config.server.host = json_string(server, "host", config.server.host);
    config.server.port = json_int(server, "port", config.server.port);
    config.server.threads = json_int(server, "threads", config.server.threads);
  }
  if (root.contains("database") && root.at("database").is_object()) {
    const auto& database = root.at("database");
    config.database.path = json_string(database, "path", config.database.path);
    config.database.migrations_dir =
        json_string(database, "migrations_dir", config.database.migrations_dir);
    config.database.busy_timeout_ms =
        json_int(database, "busy_timeout_ms", config.database.busy_timeout_ms);
    config.database.wal = json_bool(database, "wal", config.database.wal);
  }
  if (root.contains("log") && root.at("log").is_object()) {
    config.log.level = json_string(root.at("log"), "level", config.log.level);
  }
  if (root.contains("frontend") && root.at("frontend").is_object()) {
    const auto& frontend = root.at("frontend");
    config.frontend.dir = json_string(frontend, "dir", config.frontend.dir);
    config.frontend.enabled = json_bool(frontend, "enabled", config.frontend.enabled);
  }
  if (root.contains("categories") && root.at("categories").is_object()) {
    const auto& categories = root.at("categories");
    config.categories.income =
        json_string_list(categories, "income", config.categories.income);
    config.categories.expense =
        json_string_list(categories, "expense", config.categories.expense);
  }

  if (config.server.port <= 0 || config.server.port > 65535) {
    throw std::runtime_error("config server.port out of range");
  }
  if (config.server.threads <= 0) {
    config.server.threads = 1;
  }
  return config;
}

void Config::apply_log_level() const {
  set_log_level(parse_log_level(log.level));
}

}  // namespace wt

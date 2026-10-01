// 配置加载实现：从 JSON 文件读取，类型不匹配或缺失时回退默认值。
#include "config/config.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "common/logging.hpp"

namespace wt {
namespace {

using nlohmann::json;

// 读取整数字段；键不存在或类型不是整数时返回 fallback。
int json_int(const json& object, const char* key, int fallback) {
  if (object.contains(key) && object.at(key).is_number_integer()) {
    return object.at(key).get<int>();
  }
  return fallback;
}

// 读取字符串字段；键不存在或类型不是字符串时返回 fallback。
std::string json_string(const json& object, const char* key, const std::string& fallback) {
  if (object.contains(key) && object.at(key).is_string()) {
    return object.at(key).get<std::string>();
  }
  return fallback;
}

// 读取布尔字段；键不存在或类型不是布尔时返回 fallback。
bool json_bool(const json& object, const char* key, bool fallback) {
  if (object.contains(key) && object.at(key).is_boolean()) {
    return object.at(key).get<bool>();
  }
  return fallback;
}

// 严格读取关键字符串字段：键存在但类型不是字符串时抛错终止启动，而不是静默
// 回退默认值——例如 database.path 类型写错会让服务悄悄新建另一个空库。
std::string require_json_string(const json& object, const char* key,
                                const std::string& fallback, const char* context) {
  if (!object.contains(key)) {
    return fallback;
  }
  if (!object.at(key).is_string()) {
    throw std::runtime_error(std::string("config ") + context + "." + key +
                             " must be a string");
  }
  return object.at(key).get<std::string>();
}

// 严格读取关键整数字段：键存在但类型不是整数（含浮点与字符串数字）时抛错。
int require_json_int(const json& object, const char* key, int fallback, const char* context) {
  if (!object.contains(key)) {
    return fallback;
  }
  if (!object.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("config ") + context + "." + key +
                             " must be an integer");
  }
  return object.at(key).get<int>();
}

// 读取字符串数组字段；非数组时返回 fallback，数组内非字符串元素被忽略。
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
  // 分类默认值（中文名称），仅在配置未覆盖时使用。
  config.categories.expense = {"餐饮", "交通", "购物", "娱乐", "居住",
                               "医疗", "教育", "通讯", "人情", "其他"};
  config.categories.income = {"工资", "奖金", "红包", "利息收入",
                              "投资收益", "退款", "其他"};

  std::ifstream input(path);
  // 配置文件缺失不是致命错误：记警告并沿用默认值。
  if (!input.is_open()) {
    log_warn("config file not found, using defaults: " + path);
    return config;
  }

  json root;
  try {
    input >> root;
  } catch (const json::exception& error) {
    // 文件存在但解析失败属于配置错误，直接抛异常终止启动。
    throw std::runtime_error("failed to parse config file '" + path + "': " + error.what());
  }
  if (!root.is_object()) {
    throw std::runtime_error("config file root must be a JSON object: " + path);
  }

  // 逐段读取；段落缺失或类型不符时保留上文默认值。
  if (root.contains("server") && root.at("server").is_object()) {
    const auto& server = root.at("server");
    config.server.host = json_string(server, "host", config.server.host);
    config.server.port = require_json_int(server, "port", config.server.port, "server");
    config.server.threads = json_int(server, "threads", config.server.threads);
  }
  if (root.contains("database") && root.at("database").is_object()) {
    const auto& database = root.at("database");
    config.database.path =
        require_json_string(database, "path", config.database.path, "database");
    config.database.migrations_dir =
        json_string(database, "migrations_dir", config.database.migrations_dir);
    config.database.busy_timeout_ms =
        require_json_int(database, "busy_timeout_ms", config.database.busy_timeout_ms,
                         "database");
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
  config.business_timezone =
      json_string(root, "business_timezone", config.business_timezone);
  if (root.contains("auth") && root.at("auth").is_object()) {
    const auto& auth = root.at("auth");
    config.auth.mode = json_string(auth, "mode", config.auth.mode);
    config.auth.allow_unauthenticated_lan =
        json_bool(auth, "allow_unauthenticated_lan", config.auth.allow_unauthenticated_lan);
  }
  if (config.auth.mode != "required" && config.auth.mode != "disabled") {
    throw std::runtime_error("config auth.mode must be 'required' or 'disabled'");
  }

  // 端口必须是合法 TCP 端口，否则启动即失败。
  if (config.server.port <= 0 || config.server.port > 65535) {
    throw std::runtime_error("config server.port out of range");
  }
  // 负的 busy_timeout 会禁用忙等待处理，并发写入下大量报 SQLITE_BUSY。
  if (config.database.busy_timeout_ms < 0) {
    throw std::runtime_error("config database.busy_timeout_ms must be non-negative");
  }
  // 线程数非法时静默纠正为 1，避免无谓的启动失败。
  if (config.server.threads <= 0) {
    config.server.threads = 1;
  }
  return config;
}

void Config::apply_log_level() const {
  set_log_level(parse_log_level(log.level));
}

}  // namespace wt

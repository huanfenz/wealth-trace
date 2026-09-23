// 配置结构定义：对应 JSON 配置文件，缺失项使用默认值。
#pragma once

#include <string>
#include <vector>

namespace wt {

// HTTP 服务配置。
struct ServerConfig {
  std::string host = "127.0.0.1";
  int port = 8080;
  int threads = 2;
};

// SQLite 数据库配置。
struct DatabaseConfig {
  std::string path = "data/caiji.db";
  std::string migrations_dir = "migrations";
  int busy_timeout_ms = 5000;  // 数据库被占用时的等待超时（毫秒）
  bool wal = true;             // 是否启用 WAL 日志模式以提升并发读性能
};

// 日志配置。
struct LogConfig {
  std::string level = "info";
};

// 前端静态资源托管配置。
struct FrontendConfig {
  std::string dir = "frontend/dist";
  bool enabled = true;
};

// 收支分类配置：income/expense 为分类名称列表。
struct CategoryConfig {
  std::vector<std::string> income;
  std::vector<std::string> expense;
};

// 聚合全部配置项。
struct Config {
  ServerConfig server;
  DatabaseConfig database;
  LogConfig log;
  FrontendConfig frontend;
  CategoryConfig categories;
  // 业务时区（IANA 名称）。影响「业务日期」口径：债券基金赎回日推进与可赎回状态判断。
  // 审计时间戳仍以 UTC 存储。默认 Asia/Shanghai。
  std::string business_timezone = "Asia/Shanghai";

  // 从 JSON 文件加载配置：缺失的键保留默认值；
  // 文件不存在不算错误，直接返回默认配置。
  static Config load(const std::string& path);

  // 将配置中的日志级别应用到全局日志系统。
  void apply_log_level() const;
};

}  // namespace wt

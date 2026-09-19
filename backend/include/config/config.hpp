#pragma once

#include <string>
#include <vector>

namespace wt {

struct ServerConfig {
  std::string host = "127.0.0.1";
  int port = 8080;
  int threads = 2;
};

struct DatabaseConfig {
  std::string path = "data/caiji.db";
  std::string migrations_dir = "migrations";
  int busy_timeout_ms = 5000;
  bool wal = true;
};

struct LogConfig {
  std::string level = "info";
};

struct FrontendConfig {
  std::string dir = "frontend/dist";
  bool enabled = true;
};

struct CategoryConfig {
  std::vector<std::string> income;
  std::vector<std::string> expense;
};

struct Config {
  ServerConfig server;
  DatabaseConfig database;
  LogConfig log;
  FrontendConfig frontend;
  CategoryConfig categories;

  // Loads configuration from a JSON file. Missing keys keep their default
  // values. A missing file is not an error and yields the defaults.
  static Config load(const std::string& path);

  void apply_log_level() const;
};

}  // namespace wt

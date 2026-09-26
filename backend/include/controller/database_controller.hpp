#pragma once

#include <string>
#include <utility>

#include "crow.h"

namespace wt {

class Database;

class DatabaseController {
 public:
  DatabaseController(Database& database, std::string database_path,
                     std::string migrations_dir)
      : database_(database),
        database_path_(std::move(database_path)),
        migrations_dir_(std::move(migrations_dir)) {}

  void register_routes(crow::SimpleApp& app);

 private:
  Database& database_;
  std::string database_path_;
  std::string migrations_dir_;
};

}  // namespace wt

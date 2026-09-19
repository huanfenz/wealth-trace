#pragma once

#include "crow.h"

#include "service/asset_service.hpp"

namespace wt {

class Database;

class AssetController {
 public:
  explicit AssetController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  AssetService service_;
};

}  // namespace wt

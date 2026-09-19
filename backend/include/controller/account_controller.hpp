#pragma once

#include "crow.h"

#include "service/account_service.hpp"

namespace wt {

class Database;

class AccountController {
 public:
  explicit AccountController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  AccountService service_;
};

}  // namespace wt

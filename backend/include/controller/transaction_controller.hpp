#pragma once

#include "crow.h"

#include "service/transaction_service.hpp"

namespace wt {

class Database;

class TransactionController {
 public:
  explicit TransactionController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  TransactionService service_;
};

}  // namespace wt

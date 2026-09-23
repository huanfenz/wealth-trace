#pragma once

// 交易控制器声明：只负责解析请求、调用 TransactionService、序列化响应。
#include "crow.h"

#include "service/transaction_service.hpp"

namespace wt {

class Database;

// 交易 / 转账的 REST 接口：流水列表、记收入、记支出、调整、转账、单条查询、删除。
class TransactionController {
 public:
  explicit TransactionController(Database& database) : service_(database) {}

  // 注册 /api/households/<int>/transactions... 与 /api/transactions/<int> 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  TransactionService service_;
};

}  // namespace wt

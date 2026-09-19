// 交易流水（"transaction"，SQL 保留字需加双引号）数据访问：SQL 执行与行映射。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

// list()/count() 的过滤与分页条件。除 household_id 外均为可选，
// 可选字段有值时才拼接对应 WHERE 条件并绑定参数。
struct TransactionQuery {
  std::int64_t household_id = 0;
  std::optional<std::int64_t> owner_member_id;
  std::optional<std::int64_t> asset_id;
  std::optional<TransactionType> type;
  std::optional<std::string> from_time;  // inclusive
  std::optional<std::string> to_time;    // inclusive
  int limit = 200;
  int offset = 0;
};

// "transaction" 表仓储。
class TransactionRepository {
 public:
  explicit TransactionRepository(Database& database) : database_(database) {}

  // 插入一条流水，返回新行自增主键 id。
  std::int64_t create(const Transaction& transaction);
  // 按主键查询，不存在返回 std::nullopt。
  std::optional<Transaction> find_by_id(std::int64_t id);
  // 按条件分页查询，排序为 transaction_time DESC, id DESC。
  std::vector<Transaction> list(const TransactionQuery& query);
  // 按同样条件统计总数（不分页）。
  std::int64_t count(const TransactionQuery& query);
  // 统计某资产关联的流水条数。
  std::int64_t count_by_asset(std::int64_t asset_id);
  // 生成下一个转账分组 id：现有最大 transfer_group_id + 1，无记录时为 1。
  std::int64_t next_transfer_group_id();

 private:
  Database& database_;
};

}  // namespace wt

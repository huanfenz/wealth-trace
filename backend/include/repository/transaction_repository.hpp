#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

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

class TransactionRepository {
 public:
  explicit TransactionRepository(Database& database) : database_(database) {}

  std::int64_t create(const Transaction& transaction);
  std::optional<Transaction> find_by_id(std::int64_t id);
  std::vector<Transaction> list(const TransactionQuery& query);
  std::int64_t count(const TransactionQuery& query);
  std::int64_t count_by_asset(std::int64_t asset_id);
  std::int64_t next_transfer_group_id();

 private:
  Database& database_;
};

}  // namespace wt

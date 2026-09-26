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
  std::optional<std::string> from_time;
  std::optional<std::string> to_time;
  int limit = 200;
  int offset = 0;
};

class TransactionRepository {
 public:
  explicit TransactionRepository(Database& database) : database_(database) {}
  std::int64_t create(const Transaction& transaction);
  std::optional<Transaction> find_by_id(std::int64_t id);
  bool update(const Transaction& transaction);
  bool update_category(std::int64_t id, std::optional<std::int64_t> category_id,
                       const std::optional<std::string>& category,
                       const std::string& updated_at);
  bool remove(std::int64_t id);
  std::vector<Transaction> list(const TransactionQuery& query);
  std::int64_t count(const TransactionQuery& query);
  std::vector<TransactionEntry> entries(std::int64_t transaction_id);
  std::int64_t add_entry(const TransactionEntry& entry);
  void clear_entries(std::int64_t transaction_id);
  void add_investment_detail(std::int64_t transaction_id, std::int64_t asset_id,
                             InvestmentAction action, std::optional<std::int64_t> principal);
  std::optional<std::pair<std::int64_t, InvestmentAction>> investment_detail(
      std::int64_t transaction_id);
  std::int64_t count_by_asset(std::int64_t asset_id);
  std::optional<std::pair<std::string, std::string>> asset_summary(std::int64_t asset_id);

 private:
  Database& database_;
};
}  // namespace wt

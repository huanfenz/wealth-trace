#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "model/entities.hpp"
#include "repository/asset_repository.hpp"
#include "repository/transaction_repository.hpp"

namespace wt {

class Database;

struct TransferResult {
  Transaction outgoing;
  Transaction incoming;
};

class TransactionService {
 public:
  explicit TransactionService(Database& database)
      : assets_(database), transactions_(database), database_(database) {}

  Transaction record_income(std::int64_t asset_id,
                            const std::optional<std::string>& category,
                            std::int64_t amount, const std::string& transaction_time,
                            const std::optional<std::string>& remark);
  Transaction record_expense(std::int64_t asset_id,
                             const std::optional<std::string>& category,
                             std::int64_t amount, const std::string& transaction_time,
                             const std::optional<std::string>& remark);
  Transaction record_adjustment(std::int64_t asset_id, std::int64_t amount,
                                const std::string& transaction_time,
                                const std::optional<std::string>& remark);
  TransferResult transfer(std::int64_t from_asset_id, std::int64_t to_asset_id,
                          std::int64_t amount, const std::string& transaction_time,
                          const std::optional<std::string>& remark);

  std::vector<Transaction> list(const TransactionQuery& query);
  std::int64_t count(const TransactionQuery& query);
  Transaction get(std::int64_t id);

 private:
  Transaction record(TransactionType type, std::int64_t asset_id,
                     const std::optional<std::string>& category, std::int64_t amount,
                     const std::string& transaction_time,
                     const std::optional<std::string>& remark);

  AssetRepository assets_;
  TransactionRepository transactions_;
  Database& database_;
};

}  // namespace wt

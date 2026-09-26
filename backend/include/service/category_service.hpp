#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config/config.hpp"
#include "model/entities.hpp"

namespace wt {
class Database;

class CategoryService {
 public:
  CategoryService(Database& database, CategoryConfig defaults)
      : database_(database), defaults_(std::move(defaults)) {}

  std::vector<TransactionCategory> list(std::int64_t household_id,
                                        TransactionType type,
                                        bool include_inactive = false);
  TransactionCategory create(std::int64_t household_id, TransactionType type,
                             const std::string& name);
  TransactionCategory update(std::int64_t household_id, std::int64_t id, const std::string& name,
                             int sort_order);
  TransactionCategory set_active(std::int64_t household_id, std::int64_t id, bool active);

 private:
  void require_household(std::int64_t id);
  void seed_defaults(std::int64_t household_id, TransactionType type);
  Database& database_;
  CategoryConfig defaults_;
};
}  // namespace wt

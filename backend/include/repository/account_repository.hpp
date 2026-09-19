#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

class AccountRepository {
 public:
  explicit AccountRepository(Database& database) : database_(database) {}

  std::int64_t create(const Account& account);
  std::optional<Account> find_by_id(std::int64_t id);
  std::vector<Account> list_by_household(std::int64_t household_id,
                                         std::optional<std::int64_t> owner_member_id);
  bool update(const Account& account);
  bool exists(std::int64_t id);

 private:
  Database& database_;
};

}  // namespace wt

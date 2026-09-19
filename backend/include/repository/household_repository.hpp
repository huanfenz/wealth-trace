#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

class HouseholdRepository {
 public:
  explicit HouseholdRepository(Database& database) : database_(database) {}

  std::int64_t create(const Household& household);
  std::optional<Household> find_by_id(std::int64_t id);
  std::vector<Household> list();
  bool update(const Household& household);
  std::int64_t count();

 private:
  Database& database_;
};

}  // namespace wt

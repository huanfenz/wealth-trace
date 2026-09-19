#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/household_repository.hpp"

namespace wt {

class Database;

class HouseholdService {
 public:
  explicit HouseholdService(Database& database) : households_(database) {}

  Household create(const std::string& name);
  std::vector<Household> list();
  Household get(std::int64_t id);
  Household update(std::int64_t id, const std::string& name);

  // Creates the default household when the database contains none, otherwise
  // returns the existing first household. Used on first startup.
  Household ensure_default(const std::string& name);

 private:
  HouseholdRepository households_;
};

}  // namespace wt

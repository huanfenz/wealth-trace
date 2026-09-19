#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

class MemberRepository {
 public:
  explicit MemberRepository(Database& database) : database_(database) {}

  std::int64_t create(const HouseholdMember& member);
  std::optional<HouseholdMember> find_by_id(std::int64_t id);
  std::vector<HouseholdMember> list_by_household(std::int64_t household_id);
  bool update(const HouseholdMember& member);
  bool exists(std::int64_t id);
  std::int64_t count_by_household(std::int64_t household_id);

 private:
  Database& database_;
};

}  // namespace wt

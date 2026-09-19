#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/household_repository.hpp"
#include "repository/member_repository.hpp"

namespace wt {

class Database;

class MemberService {
 public:
  explicit MemberService(Database& database)
      : households_(database), members_(database) {}

  HouseholdMember create(std::int64_t household_id, const std::string& name,
                         MemberRole role, MemberStatus status);
  std::vector<HouseholdMember> list(std::int64_t household_id);
  HouseholdMember get(std::int64_t id);
  HouseholdMember update(std::int64_t id, const std::string& name, MemberRole role,
                         MemberStatus status);

 private:
  void require_household(std::int64_t household_id);

  HouseholdRepository households_;
  MemberRepository members_;
};

}  // namespace wt

#include "service/member_service.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"

namespace wt {

void MemberService::require_household(std::int64_t household_id) {
  if (!households_.find_by_id(household_id).has_value()) {
    throw not_found("household not found");
  }
}

HouseholdMember MemberService::create(std::int64_t household_id,
                                      const std::string& name, MemberRole role,
                                      MemberStatus status) {
  require_household(household_id);
  HouseholdMember member;
  member.household_id = household_id;
  member.name = strings::require_text(name, "name", 100);
  member.role = role;
  member.status = status;
  member.created_at = time_util::now_iso8601();
  member.updated_at = member.created_at;
  member.id = members_.create(member);
  return member;
}

std::vector<HouseholdMember> MemberService::list(std::int64_t household_id) {
  require_household(household_id);
  return members_.list_by_household(household_id);
}

HouseholdMember MemberService::get(std::int64_t id) {
  const auto member = members_.find_by_id(id);
  if (!member.has_value()) {
    throw not_found("member not found");
  }
  return *member;
}

HouseholdMember MemberService::update(std::int64_t id, const std::string& name,
                                      MemberRole role, MemberStatus status) {
  HouseholdMember member = get(id);
  member.name = strings::require_text(name, "name", 100);
  member.role = role;
  member.status = status;
  member.updated_at = time_util::now_iso8601();
  members_.update(member);
  return member;
}

}  // namespace wt

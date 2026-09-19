// 成员服务：家庭成员（HouseholdMember）的增删改查，成员是账户与资产的属主。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/household_repository.hpp"
#include "repository/member_repository.hpp"

namespace wt {

class Database;

// 成员服务：维护 HouseholdMember。成员只归属于一个家庭，账户/资产通过
// owner_member_id 引用成员；本服务不触发金额变动。
class MemberService {
 public:
  explicit MemberService(Database& database)
      : households_(database), members_(database) {}

  // 在指定家庭下新建成员：先校验家庭存在（否则 not_found），name 去空白且
  // 长度不超过 100（否则 invalid_request）。role/status 由调用方给定。
  HouseholdMember create(std::int64_t household_id, const std::string& name,
                         MemberRole role, MemberStatus status);

  // 列出某家庭的全部成员；家庭不存在抛 not_found。
  std::vector<HouseholdMember> list(std::int64_t household_id);

  // 按 id 获取成员；不存在抛 not_found。
  HouseholdMember get(std::int64_t id);

  // 更新成员姓名/角色/状态；成员不存在抛 not_found，name 校验同 create。
  HouseholdMember update(std::int64_t id, const std::string& name, MemberRole role,
                         MemberStatus status);

 private:
  // 前置校验：家庭必须存在，否则抛 not_found。
  void require_household(std::int64_t household_id);

  HouseholdRepository households_;
  MemberRepository members_;
};

}  // namespace wt

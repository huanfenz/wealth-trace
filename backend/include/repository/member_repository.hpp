// 家庭成员（household_member）数据访问：仅封装该表的 SQL 执行与行映射。
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

// household_member 表仓储。所有方法均使用预处理语句绑定参数。
class MemberRepository {
 public:
  explicit MemberRepository(Database& database) : database_(database) {}

  // 插入一个成员，返回新行自增主键 id。
  std::int64_t create(const HouseholdMember& member);
  // 按主键查询，不存在返回 std::nullopt。
  std::optional<HouseholdMember> find_by_id(std::int64_t id);
  // 查询某家庭下的全部成员，按 id 升序。
  std::vector<HouseholdMember> list_by_household(std::int64_t household_id);
  // 按 id 更新 name/role/status/updated_at；返回是否真正改动了行。
  bool update(const HouseholdMember& member);
  // 判断主键是否存在。
  bool exists(std::int64_t id);
  // 统计某家庭下的成员数量。
  std::int64_t count_by_household(std::int64_t household_id);

 private:
  Database& database_;
};

}  // namespace wt

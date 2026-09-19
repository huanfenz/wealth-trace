// 家庭（household）数据访问：仅封装 household 表的 SQL 执行与行映射。
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

// household 表仓储。所有方法均使用预处理语句绑定参数。
class HouseholdRepository {
 public:
  explicit HouseholdRepository(Database& database) : database_(database) {}

  // 插入一个家庭，返回新行自增主键 id。
  std::int64_t create(const Household& household);
  // 按主键查询，不存在返回 std::nullopt。
  std::optional<Household> find_by_id(std::int64_t id);
  // 返回全部家庭，按 id 升序。
  std::vector<Household> list();
  // 按 id 更新 name/updated_at；返回是否真正改动了行。
  bool update(const Household& household);
  // 返回家庭总数。
  std::int64_t count();

 private:
  Database& database_;
};

}  // namespace wt

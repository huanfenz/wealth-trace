// 账户（account）数据访问：仅封装该表的 SQL 执行与行映射。
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

// account 表仓储。owner_member_id 冗余自账户归属成员，供免联表查询。
class AccountRepository {
 public:
  explicit AccountRepository(Database& database) : database_(database) {}

  // 插入一个账户，返回新行自增主键 id。
  std::int64_t create(const Account& account);
  // 按主键查询，不存在返回 std::nullopt。
  std::optional<Account> find_by_id(std::int64_t id);
  // 查询某家庭下的账户；owner_member_id 有值时追加成员过滤条件。
  std::vector<Account> list_by_household(std::int64_t household_id,
                                         std::optional<std::int64_t> owner_member_id);
  // 按 id 更新账户字段；返回是否真正改动了行。
  bool update(const Account& account);
  // 判断主键是否存在。
  bool exists(std::int64_t id);

 private:
  Database& database_;
};

}  // namespace wt

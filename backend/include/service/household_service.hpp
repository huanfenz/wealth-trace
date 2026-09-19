// 家庭服务：家庭（Household）这一顶层聚合单元的创建、查询与重命名。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/household_repository.hpp"

namespace wt {

class Database;

// 家庭服务：维护 Household 生命周期。家庭是成员、账户、资产、交易的归属根，
// 本服务只做文本校验与持久化，不涉及金额或事务。
class HouseholdService {
 public:
  explicit HouseholdService(Database& database) : households_(database) {}

  // 新建家庭：name 去首尾空白且长度不超过 100，否则抛 invalid_request。
  // 返回带自增 id、created_at/updated_at 的家庭。
  Household create(const std::string& name);

  // 列出全部家庭。
  std::vector<Household> list();

  // 按 id 获取家庭；不存在抛 not_found。
  Household get(std::int64_t id);

  // 重命名家庭：校验同 create，不存在抛 not_found，成功后刷新 updated_at。
  Household update(std::int64_t id, const std::string& name);

  // 首次启动初始化用：库中没有家庭时按 name 创建并返回，否则返回列表中
  // 的第一个家庭，保证系统始终至少存在一个家庭。
  Household ensure_default(const std::string& name);

 private:
  HouseholdRepository households_;
};

}  // namespace wt

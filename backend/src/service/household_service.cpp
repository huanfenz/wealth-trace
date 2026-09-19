// 家庭服务实现：家庭的新建、查询、重命名与首次启动的默认家庭保证。
#include "service/household_service.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"

namespace wt {

Household HouseholdService::create(const std::string& name) {
  Household household;
  // 名称做去空白与长度校验；创建/更新时间统一用 UTC 格式，二者初始相同。
  household.name = strings::require_text(name, "name", 100);
  household.created_at = time_util::now_iso8601();
  household.updated_at = household.created_at;
  household.id = households_.create(household);
  return household;
}

std::vector<Household> HouseholdService::list() { return households_.list(); }

Household HouseholdService::get(std::int64_t id) {
  const auto household = households_.find_by_id(id);
  if (!household.has_value()) {
    throw not_found("household not found");
  }
  return *household;
}

Household HouseholdService::update(std::int64_t id, const std::string& name) {
  // 先取原记录（不存在即抛 not_found），只改名称与 updated_at。
  Household household = get(id);
  household.name = strings::require_text(name, "name", 100);
  household.updated_at = time_util::now_iso8601();
  households_.update(household);
  return household;
}

Household HouseholdService::ensure_default(const std::string& name) {
  // 空库时懒创建默认家庭；已有家庭则复用第一个，使调用方幂等，
  // 不会因重复启动而制造出多个家庭。
  if (households_.count() == 0) {
    return create(name);
  }
  const auto all = households_.list();
  return all.front();
}

}  // namespace wt

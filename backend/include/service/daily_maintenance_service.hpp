// 每日资产维护服务：让「与时间相关的状态」收敛到今天应有的状态。
// 目前负责滚动持有债券基金 next_redeem_date 的推进与自动续存定期存款的续期；
// 启动时若当天未执行则补跑，也可由用户手动预览并触发。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "repository/asset_repository.hpp"
#include "repository/system_state_repository.hpp"

namespace wt {

class Database;

// 单条维护变更：某个已有资产某日期字段将推进的前后值。
struct MaintenanceChange {
  std::int64_t asset_id = 0;
  std::string asset_name;
  std::string asset_type;  // BOND_FUND / TERM_DEPOSIT
  std::string field;       // next_redeem_date / start_date / maturity_date
  std::string before;
  std::string after;
};

// 一次维护计划：按业务日期推导「如果现在执行维护会改哪些资产」，不落库。
struct MaintenancePlan {
  std::string business_date;
  bool required = false;
  std::vector<MaintenanceChange> changes;
};

// 一次维护执行结果：各类被推进的资产条数。
struct MaintenanceResult {
  std::string business_date;
  std::int64_t bond_funds = 0;
  std::int64_t term_deposits = 0;
};

// 每日维护：把资产的时间状态推进到今天，而不是逐日回放。
// 设计要点：
//   * 判断必须用 today > next_redeem_date，today == 当天仍是有效赎回日；
//   * 单次任务用 while 跨过多个周期，具备幂等性；
//   * 整个任务在一个事务内完成，成功后写入 daily_maintenance_last_run。
class DailyAssetMaintenanceService {
 public:
  explicit DailyAssetMaintenanceService(Database& database)
      : database_(database), states_(database), assets_(database) {}

  // 当天尚未执行时执行一次维护（启动补跑用）。返回是否实际执行。
  bool run_if_due();

  // 强制执行一次维护；幂等，可重复调用。异常时回滚且不更新 last_run。
  MaintenanceResult run();

  // 预览当前业务日期下执行维护将产生的变更，不修改任何数据。
  MaintenancePlan preview();

 private:
  // 推进所有 ROLLING 债基的 next_redeem_date，返回被修改的条数。
  std::int64_t update_bond_funds(const std::string& today);
  // 推进自动续存定期存款的本期起止日期，返回被修改的条数。
  std::int64_t update_term_deposits(const std::string& today);
  // 取资产名称用于预览展示；资产不存在时返回空串。
  std::string asset_name(std::int64_t asset_id);

  Database& database_;
  SystemStateRepository states_;
  AssetRepository assets_;
};

}  // namespace wt

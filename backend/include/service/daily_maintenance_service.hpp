// 每日资产维护服务：让「与时间相关的状态」收敛到今天应有的状态。
// 目前负责滚动持有债券基金 next_redeem_date 的推进；启动时若当天未执行则补跑。
#pragma once

#include <cstdint>
#include <string>

#include "repository/asset_repository.hpp"
#include "repository/system_state_repository.hpp"

namespace wt {

class Database;

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
  void run();

 private:
  // 推进所有 ROLLING 债基的 next_redeem_date，返回被修改的条数。
  std::int64_t update_bond_funds(const std::string& today);
  // 推进自动续存定期存款的本期起止日期，返回被修改的条数。
  std::int64_t update_term_deposits(const std::string& today);

  Database& database_;
  SystemStateRepository states_;
  AssetRepository assets_;
};

}  // namespace wt

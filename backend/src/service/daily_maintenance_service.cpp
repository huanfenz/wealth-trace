// 每日资产维护服务实现：滚动债基赎回日推进 + 每日维护游标。
#include "service/daily_maintenance_service.hpp"

#include <optional>
#include <string>

#include "common/logging.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "utils/term_date.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

// system_state 中记录每日维护最近一次成功执行日期（"YYYY-MM-DD"）的键。
constexpr const char* kLastRunKey = "daily_maintenance_last_run";

}  // namespace

bool DailyAssetMaintenanceService::run_if_due() {
  // 锁覆盖「检查 + 执行」全过程，避免启动补跑与定时任务并发重复执行。
  // database_.mutex() 是可重入锁，run() 内部会再次加锁。
  std::scoped_lock lock(database_.mutex());
  const std::string today = time_util::business_today();
  const auto last = states_.get(kLastRunKey);
  if (last.has_value() && *last == today) {
    return false;
  }
  run();
  return true;
}

void DailyAssetMaintenanceService::run() {
  std::scoped_lock lock(database_.mutex());
  const std::string today = time_util::business_today();
  // 更新与 last_run 写入必须同属一个事务：任一步失败都整体回滚，
  // last_run 保持旧值，下一次启动/调度可以重跑。
  TransactionGuard transaction(database_);
  const std::int64_t bond_funds = update_bond_funds(today);
  const std::int64_t term_deposits = update_term_deposits(today);
  states_.set(kLastRunKey, today);
  transaction.commit();
  log_info("daily maintenance done for " + today + ": bond funds advanced " +
           std::to_string(bond_funds) + ", term deposits rolled " +
           std::to_string(term_deposits));
}

std::int64_t DailyAssetMaintenanceService::update_bond_funds(const std::string& today) {
  std::int64_t updated = 0;
  for (const auto& fund : assets_.list_rolling_bond_funds()) {
    // 滚动型正常都有 next_redeem_date；缺失数据直接跳过，不臆造。
    if (!fund.next_redeem_date.has_value()) {
      continue;
    }
    std::string next = *fund.next_redeem_date;
    bool changed = false;
    // 关键：必须用 today > next。today == next 代表今天仍是有效赎回日，
    // 若用 >= 会在赎回日 0 点直接滚入下一期，用户就看不到「今日可赎回」。
    while (today > next) {
      next = time_util::add_days(next, fund.holding_period_days);
      changed = true;
    }
    if (changed) {
      assets_.update_bond_fund_next_redeem_date(fund.asset_id, next);
      ++updated;
    }
  }
  return updated;
}

std::int64_t DailyAssetMaintenanceService::update_term_deposits(
    const std::string& today) {
  std::int64_t updated = 0;
  for (const auto& deposit : assets_.list_auto_rollover_term_deposits()) {
    // 缺少必要字段的历史/异常数据直接跳过，不臆造。
    if (!deposit.maturity_date.has_value() || !deposit.term_value.has_value() ||
        !deposit.term_unit.has_value() || *deposit.term_value <= 0) {
      continue;
    }
    std::string start = deposit.start_date.value_or(*deposit.maturity_date);
    std::string maturity = *deposit.maturity_date;
    bool changed = false;
    // 到期即续存：与滚动债基的 today > next 不同，自动续存用 today >= maturity，
    // 到期当天即进入下一存期。旧 maturity 作为新一期起始日，避免停机导致周期漂移。
    while (today >= maturity) {
      start = maturity;
      maturity = time_util::add_term(maturity, *deposit.term_value, *deposit.term_unit);
      changed = true;
    }
    if (changed) {
      assets_.update_term_deposit_period(deposit.asset_id, start, maturity);
      ++updated;
    }
  }
  return updated;
}

}  // namespace wt

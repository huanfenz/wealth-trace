// 每日资产维护服务实现：滚动债基赎回日推进 + 自动续存定存续期 + 每日维护游标。
#include "service/daily_maintenance_service.hpp"

#include <optional>
#include <string>
#include <utility>

#include "common/logging.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "utils/maintenance_rules.hpp"
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

MaintenanceResult DailyAssetMaintenanceService::run() {
  std::scoped_lock lock(database_.mutex());
  const std::string today = time_util::business_today();
  // 更新与 last_run 写入必须同属一个事务：任一步失败都整体回滚，
  // last_run 保持旧值，下一次启动/调度可以重跑。
  TransactionGuard transaction(database_);
  MaintenanceResult result;
  result.business_date = today;
  result.bond_funds = update_bond_funds(today);
  result.term_deposits = update_term_deposits(today);
  states_.set(kLastRunKey, today);
  transaction.commit();
  log_info("daily maintenance done for " + today + ": bond funds advanced " +
           std::to_string(result.bond_funds) + ", term deposits rolled " +
           std::to_string(result.term_deposits));
  return result;
}

MaintenancePlan DailyAssetMaintenanceService::preview() {
  std::scoped_lock lock(database_.mutex());
  const std::string today = time_util::business_today();
  MaintenancePlan plan;
  plan.business_date = today;

  for (const auto& fund : assets_.list_rolling_bond_funds()) {
    // 滚动型正常都有 next_redeem_date；缺失数据直接跳过，不臆造。
    if (!fund.next_redeem_date.has_value()) {
      continue;
    }
    const auto advanced = maintenance_rules::advance_bond_fund_next(
        *fund.next_redeem_date, fund.holding_period_days, today);
    if (!advanced.has_value()) {
      continue;
    }
    MaintenanceChange change;
    change.asset_id = fund.asset_id;
    change.asset_name = asset_name(fund.asset_id);
    change.asset_type = "BOND_FUND";
    change.field = "next_redeem_date";
    change.before = *fund.next_redeem_date;
    change.after = *advanced;
    plan.changes.push_back(std::move(change));
  }

  for (const auto& deposit : assets_.list_auto_rollover_term_deposits()) {
    // 缺少必要字段的历史/异常数据直接跳过，不臆造。
    if (!deposit.maturity_date.has_value() || !deposit.term_value.has_value() ||
        !deposit.term_unit.has_value() || *deposit.term_value <= 0) {
      continue;
    }
    const auto advanced = maintenance_rules::advance_term_period(
        *deposit.maturity_date, *deposit.term_value, *deposit.term_unit, today);
    if (!advanced.has_value()) {
      continue;
    }
    const std::string start = deposit.start_date.value_or(*deposit.maturity_date);
    const std::string name = asset_name(deposit.asset_id);
    plan.changes.push_back(MaintenanceChange{deposit.asset_id, name, "TERM_DEPOSIT",
                                             "start_date", start, advanced->start});
    plan.changes.push_back(MaintenanceChange{deposit.asset_id, name, "TERM_DEPOSIT",
                                             "maturity_date", *deposit.maturity_date,
                                             advanced->maturity});
  }

  plan.required = !plan.changes.empty();
  return plan;
}

std::string DailyAssetMaintenanceService::asset_name(std::int64_t asset_id) {
  const auto asset = assets_.find_by_id(asset_id);
  return asset.has_value() ? asset->name : std::string();
}

std::int64_t DailyAssetMaintenanceService::update_bond_funds(const std::string& today) {
  std::int64_t updated = 0;
  for (const auto& fund : assets_.list_rolling_bond_funds()) {
    if (!fund.next_redeem_date.has_value()) {
      continue;
    }
    const auto advanced = maintenance_rules::advance_bond_fund_next(
        *fund.next_redeem_date, fund.holding_period_days, today);
    if (!advanced.has_value()) {
      continue;
    }
    assets_.update_bond_fund_next_redeem_date(fund.asset_id, *advanced);
    ++updated;
  }
  return updated;
}

std::int64_t DailyAssetMaintenanceService::update_term_deposits(
    const std::string& today) {
  std::int64_t updated = 0;
  for (const auto& deposit : assets_.list_auto_rollover_term_deposits()) {
    if (!deposit.maturity_date.has_value() || !deposit.term_value.has_value() ||
        !deposit.term_unit.has_value() || *deposit.term_value <= 0) {
      continue;
    }
    const auto advanced = maintenance_rules::advance_term_period(
        *deposit.maturity_date, *deposit.term_value, *deposit.term_unit, today);
    if (!advanced.has_value()) {
      continue;
    }
    assets_.update_term_deposit_period(deposit.asset_id, advanced->start,
                                       advanced->maturity);
    ++updated;
  }
  return updated;
}

}  // namespace wt

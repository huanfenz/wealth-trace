#pragma once

// DTO 序列化：把领域实体 / 查询结果转换为对外 JSON。
// 约束：金额字段单位均为「分」（整数）；负债金额以负值表示；
// 可选字段无值时输出 null；明细块（term_deposit/stock_fund/bond/insurance）
// 与资产类型不匹配时输出 null。
#include <nlohmann/json.hpp>

#include "model/entities.hpp"
#include "model/recurring_investment.hpp"
#include "repository/statistics_repository.hpp"
#include "service/account_service.hpp"
#include "service/asset_service.hpp"
#include "service/daily_maintenance_service.hpp"
#include "service/statistics_service.hpp"
#include "service/transaction_service.hpp"

namespace wt::dto {

// 家庭。
nlohmann::json to_json(const Household& household);
// 家庭成员（role/status 以枚举名输出）。
nlohmann::json to_json(const HouseholdMember& member);
// 账户基础信息（可选字段：机构、掩码卡号、备注）。
nlohmann::json to_json(const Account& account);
// 账户视图 = 账户基础信息 + balance 余额（分）+ asset_count 资产数。
nlohmann::json to_json(const AccountView& view);
// 资产基础信息（current_balance/opening_balance 单位为分，负债为负值）。
nlohmann::json to_json(const Asset& asset);
// 定期存款明细（利率为定点整数 RATE_SCALE=1000000）。
nlohmann::json to_json(const TermDepositDetail& detail);
// 股票基金明细。
nlohmann::json to_json(const StockFundDetail& detail);
// 债券基金明细（持有方式、赎回日期；收益率为定点整数）。
nlohmann::json to_json(const BondFundDetail& detail);
nlohmann::json to_json(const FlexibleTermDetail& detail);
nlohmann::json to_json(const CommercialPensionDetail& detail);
// 保险明细（金额单位均为分）。
nlohmann::json to_json(const InsuranceDetail& detail);
// 资产聚合包：资产基础信息 + 各类型明细块（不适用的明细块为 null）。
nlohmann::json to_json(const AssetBundle& bundle);
// 创建资产时的「添加时维护」预览（不落库）。
nlohmann::json to_json(const CreateMaintenancePreview& preview);
// 每日维护预览计划（不落库）。
nlohmann::json to_json(const MaintenancePlan& plan);
// 每日维护执行结果（各类被推进的资产数）。
nlohmann::json to_json(const MaintenanceResult& result);
// 交易流水（金额为分，收支方向由 type 决定；时间 UTC 字符串）。
nlohmann::json to_json(const Transaction& transaction);
// 转账结果：同时返回转出与转入两条流水。
nlohmann::json to_json(const TransferResult& result);
nlohmann::json to_json(const RecurringInvestmentPlan& plan);
nlohmann::json to_json(const RecurringInvestmentExecution& execution);

// 按成员聚合的金额（金额为分）。
nlohmann::json to_json(const NamedAmount& amount);
// 按资产类型聚合的金额（金额为分）。
nlohmann::json to_json(const TypeAmount& amount);
// 按分类聚合的金额（金额为分）。
nlohmann::json to_json(const CategoryAmount& amount);
// 单月收支：month（YYYY-MM）、收入 / 支出 / 结余（分）。
nlohmann::json to_json(const MonthlyIncomeExpense& amount);
// 家庭总览：资产 / 负债 / 净资产 / 当月收支及多维聚合。
nlohmann::json to_json(const HouseholdOverview& overview);
// 区间统计：收支合计、按成员与按分类的聚合。
nlohmann::json to_json(const PeriodStatistics& statistics);

}  // namespace wt::dto

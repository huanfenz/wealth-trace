// DTO 序列化实现。金额一律为「分」，负债为负值；可选值缺省时输出 JSON null。
#include "dto/serialization.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "utils/time_util.hpp"

namespace wt::dto {
namespace {

// 可选字符串 -> JSON：有值输出字符串，无值输出 null。
nlohmann::json optional_text(const std::optional<std::string>& value) {
  return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

// 可选整数 -> JSON：有值输出数字，无值输出 null。
nlohmann::json optional_int(const std::optional<std::int64_t>& value) {
  return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

// 债券基金赎回状态：按业务日期实时推导，不落库，避免状态字段与日期字段不一致。
//   MIN_HOLDING：today < first -> LOCKED；today >= first -> REDEEMABLE
//   ROLLING    ：today < next  -> LOCKED；today == next -> REDEEMABLE_TODAY；
//                today > next  -> PENDING（每日维护尚未推进）
std::string bond_fund_status(const BondFundDetail& detail, const std::string& today) {
  if (detail.holding_mode == HoldingMode::Rolling) {
    if (!detail.next_redeem_date.has_value()) {
      return "UNKNOWN";
    }
    if (today < *detail.next_redeem_date) {
      return "LOCKED";
    }
    if (today == *detail.next_redeem_date) {
      return "REDEEMABLE_TODAY";
    }
    return "PENDING";
  }
  if (!detail.first_redeem_date.has_value()) {
    return "UNKNOWN";
  }
  return today < *detail.first_redeem_date ? "LOCKED" : "REDEEMABLE";
}

// 距离可赎回日的天数（业务日期 -> 目标日期的有符号差）；目标日期为空时返回 null。
std::optional<std::int64_t> bond_fund_days_until(const BondFundDetail& detail,
                                                 const std::string& today) {
  const auto& target = detail.holding_mode == HoldingMode::Rolling
                           ? detail.next_redeem_date
                           : detail.first_redeem_date;
  if (!target.has_value()) {
    return std::nullopt;
  }
  return time_util::days_between(today, *target);
}

// 定期存款到期状态（按业务日期实时推导，不落库）：
//   today < maturity  -> ACTIVE（存续中）
//   today >= maturity -> MATURED（未自动续存，等待用户处理）
//                        / ACTIVE（自动续存，已进入下一存期）
std::string term_deposit_status(const TermDepositDetail& detail, const std::string& today) {
  if (!detail.maturity_date.has_value()) {
    return "UNKNOWN";
  }
  if (today < *detail.maturity_date) {
    return "ACTIVE";
  }
  return detail.auto_rollover ? "ACTIVE" : "MATURED";
}

// 距离到期日的天数（业务日期 -> 到期日的有符号差）；到期日为空时返回 null。
std::optional<std::int64_t> term_deposit_days_until(const TermDepositDetail& detail,
                                                    const std::string& today) {
  if (!detail.maturity_date.has_value()) {
    return std::nullopt;
  }
  return time_util::days_between(today, *detail.maturity_date);
}

}  // namespace

// 家庭：id / 名称 / 创建与更新时间。
nlohmann::json to_json(const Household& household) {
  return {{"id", household.id},
          {"name", household.name},
          {"created_at", household.created_at},
          {"updated_at", household.updated_at}};
}

// 成员：role / status 以枚举名字符串输出。
nlohmann::json to_json(const HouseholdMember& member) {
  return {{"id", member.id},
          {"household_id", member.household_id},
          {"name", member.name},
          {"role", std::string(to_string(member.role))},
          {"status", std::string(to_string(member.status))},
          {"created_at", member.created_at},
          {"updated_at", member.updated_at}};
}

// 账户：type 为枚举名，机构 / 掩码卡号 / 备注为可选（无值输出 null）。
nlohmann::json to_json(const Account& account) {
  return {{"id", account.id},
          {"household_id", account.household_id},
          {"owner_member_id", account.owner_member_id},
          {"name", account.name},
          {"type", std::string(to_string(account.type))},
          {"institution_name", optional_text(account.institution_name)},
          {"account_no_masked", optional_text(account.account_no_masked)},
          {"remark", optional_text(account.remark)},
          {"enabled", account.enabled},
          {"created_at", account.created_at},
          {"updated_at", account.updated_at}};
}

// 账户视图：在账户 JSON 上追加 balance（余额，分）与 asset_count（资产数）。
nlohmann::json to_json(const AccountView& view) {
  auto json = to_json(view.account);
  json["balance"] = view.balance;
  json["asset_count"] = view.asset_count;
  return json;
}

// 资产：asset_type / status 为枚举名；opening_balance 与 current_balance
// 单位为分，负债类资产为负值；备注可选。
nlohmann::json to_json(const Asset& asset) {
  return {{"id", asset.id},
          {"household_id", asset.household_id},
          {"owner_member_id", asset.owner_member_id},
          {"account_id", asset.account_id},
          {"name", asset.name},
          {"asset_type", std::string(to_string(asset.asset_type))},
          {"opening_balance", asset.opening_balance},
          {"current_balance", asset.current_balance},
          {"status", std::string(to_string(asset.status))},
          {"remark", optional_text(asset.remark)},
          {"created_at", asset.created_at},
          {"updated_at", asset.updated_at}};
}

// 定期存款明细：annual_interest_rate 年利率（定点整数，除以 RATE_SCALE=1000000 得到小数），
// 日期为可空字符串，term_unit 为枚举名，term_value 存期数值，auto_rollover 是否自动转存。
// 本金统一取 asset.opening_balance，不再在明细里返回。
nlohmann::json to_json(const TermDepositDetail& detail) {
  const std::string today = time_util::business_today();
  nlohmann::json json;
  json["asset_id"] = detail.asset_id;
  json["annual_interest_rate"] = detail.annual_interest_rate;
  json["start_date"] = optional_text(detail.start_date);
  json["maturity_date"] = optional_text(detail.maturity_date);
  json["term_value"] = optional_int(detail.term_value);
  json["term_unit"] = detail.term_unit.has_value()
                          ? nlohmann::json(std::string(to_string(*detail.term_unit)))
                          : nlohmann::json(nullptr);
  json["interest_type"] = optional_text(detail.interest_type);
  json["auto_rollover"] = detail.auto_rollover;
  json["maturity_action"] = optional_text(detail.maturity_action);
  // 只读派生字段：由后端按业务日期推导，前端直接展示。
  json["status"] = term_deposit_status(detail, today);
  json["days_until_maturity"] = optional_int(term_deposit_days_until(detail, today));
  return json;
}

// 基金明细：代码 / 类型与锁定期起止日期均可选；名称统一使用 asset.name。
nlohmann::json to_json(const FundDetail& detail) {
  return {{"asset_id", detail.asset_id},
          {"fund_code", optional_text(detail.fund_code)},
          {"fund_type", optional_text(detail.fund_type)},
          {"lock_start_date", optional_text(detail.lock_start_date)},
          {"lock_end_date", optional_text(detail.lock_end_date)}};
}

// 债券明细：annual_coupon_rate 票面利率（定点整数）；名称用 asset.name，本金用 opening_balance。
nlohmann::json to_json(const BondDetail& detail) {
  return {{"asset_id", detail.asset_id},
          {"bond_code", optional_text(detail.bond_code)},
          {"annual_coupon_rate", detail.annual_coupon_rate},
          {"purchase_date", optional_text(detail.purchase_date)},
          {"maturity_date", optional_text(detail.maturity_date)},
          {"lock_end_date", optional_text(detail.lock_end_date)}};
}

// 债券基金明细：预期年化收益率（定点整数）、持有方式枚举名，
// 以及购买/首次赎回/下次赎回/最终到期日期（均可空）。名称用 asset.name，本金用 opening_balance。
nlohmann::json to_json(const BondFundDetail& detail) {
  const std::string today = time_util::business_today();
  return {{"asset_id", detail.asset_id},
          {"fund_code", optional_text(detail.fund_code)},
          {"expected_annual_yield_rate", optional_int(detail.expected_annual_yield_rate)},
          {"purchase_date", detail.purchase_date},
          {"holding_mode", std::string(to_string(detail.holding_mode))},
          {"holding_period_days", detail.holding_period_days},
          {"first_redeem_date", optional_text(detail.first_redeem_date)},
          {"next_redeem_date", optional_text(detail.next_redeem_date)},
          {"maturity_date", optional_text(detail.maturity_date)},
          // 以下为按业务日期实时推导的只读字段，供前端直接展示，避免前端用浏览器日期推断。
          {"status", bond_fund_status(detail, today)},
          {"days_until_redeem", optional_int(bond_fund_days_until(detail, today))}};
}

// 保险明细：保费 / 已缴 / 保额均为「分」，payment_years 缴费年限可选。
nlohmann::json to_json(const InsuranceDetail& detail) {
  return {{"asset_id", detail.asset_id},
          {"policy_no", optional_text(detail.policy_no)},
          {"insurance_company", optional_text(detail.insurance_company)},
          {"product_name", optional_text(detail.product_name)},
          {"insurance_type", optional_text(detail.insurance_type)},
          {"effective_date", optional_text(detail.effective_date)},
          {"maturity_date", optional_text(detail.maturity_date)},
          {"annual_premium", detail.annual_premium},
          {"total_paid_premium", detail.total_paid_premium},
          {"insured_amount", detail.insured_amount},
          {"payment_years", optional_int(detail.payment_years)}};
}

// 资产聚合包：先展开资产基础信息，再挂四个明细块；
// 资产类型用不到的明细块输出 null，前端据此展示对应编辑区。
nlohmann::json to_json(const AssetBundle& bundle) {
  auto json = to_json(bundle.asset);
  json["term_deposit"] = bundle.term_deposit.has_value()
                             ? to_json(*bundle.term_deposit)
                             : nlohmann::json(nullptr);
  json["fund"] = bundle.fund.has_value() ? to_json(*bundle.fund) : nlohmann::json(nullptr);
  json["bond"] = bundle.bond.has_value() ? to_json(*bundle.bond) : nlohmann::json(nullptr);
  json["bond_fund"] = bundle.bond_fund.has_value() ? to_json(*bundle.bond_fund)
                                                    : nlohmann::json(nullptr);
  json["insurance"] = bundle.insurance.has_value() ? to_json(*bundle.insurance)
                                                    : nlohmann::json(nullptr);
  return json;
}

// 创建时维护预览：required 是否需要维护，changes 为各字段推进前后值。
nlohmann::json to_json(const CreateMaintenancePreview& preview) {
  nlohmann::json changes = nlohmann::json::array();
  for (const auto& change : preview.changes) {
    changes.push_back({{"field", change.field},
                       {"before", change.before},
                       {"after", change.after}});
  }
  return {{"required", preview.required},
          {"asset_type", std::string(to_string(preview.asset_type))},
          {"changes", changes}};
}

// 每日维护预览计划：业务日期 + 是否需要维护 + 全部变更（含资产 id/名称/类型）。
nlohmann::json to_json(const MaintenancePlan& plan) {
  nlohmann::json changes = nlohmann::json::array();
  for (const auto& change : plan.changes) {
    changes.push_back({{"asset_id", change.asset_id},
                       {"asset_name", change.asset_name},
                       {"asset_type", change.asset_type},
                       {"field", change.field},
                       {"before", change.before},
                       {"after", change.after}});
  }
  return {{"business_date", plan.business_date},
          {"required", plan.required},
          {"changes", changes}};
}

// 每日维护执行结果：业务日期 + 各类被推进的资产条数。
nlohmann::json to_json(const MaintenanceResult& result) {
  return {{"business_date", result.business_date},
          {"bond_funds", result.bond_funds},
          {"term_deposits", result.term_deposits}};
}

// 交易流水：amount 为分；category 分类可选；transfer_group_id 关联同一笔转账的
// 两条流水；balance_before/after 为交易前后资产余额（分，可为空）；
// transaction_time 为 UTC "YYYY-MM-DD HH:MM:SS"；status 为枚举名。
nlohmann::json to_json(const Transaction& transaction) {
  return {{"id", transaction.id},
          {"household_id", transaction.household_id},
          {"owner_member_id", transaction.owner_member_id},
          {"asset_id", transaction.asset_id},
          {"type", std::string(to_string(transaction.type))},
          {"category", optional_text(transaction.category)},
          {"amount", transaction.amount},
          {"transfer_group_id", optional_int(transaction.transfer_group_id)},
          {"balance_before", optional_int(transaction.balance_before)},
          {"balance_after", optional_int(transaction.balance_after)},
          {"transaction_time", transaction.transaction_time},
          {"remark", optional_text(transaction.remark)},
          {"status", std::string(to_string(transaction.status))},
          {"created_at", transaction.created_at},
          {"updated_at", transaction.updated_at}};
}

// 转账结果：outgoing 转出流水、incoming 转入流水。
nlohmann::json to_json(const TransferResult& result) {
  return {{"outgoing", to_json(result.outgoing)}, {"incoming", to_json(result.incoming)}};
}

// 按成员聚合：成员 id / 名称 / 金额（分）。
nlohmann::json to_json(const NamedAmount& amount) {
  return {{"id", amount.id}, {"name", amount.name}, {"amount", amount.amount}};
}

// 按资产类型聚合：类型名 + 金额（分）。
nlohmann::json to_json(const TypeAmount& amount) {
  return {{"asset_type", std::string(to_string(amount.type))}, {"amount", amount.amount}};
}

// 按收支分类聚合：分类名 + 金额（分）。
nlohmann::json to_json(const CategoryAmount& amount) {
  return {{"category", amount.category}, {"amount", amount.amount}};
}

// 单月收支：month 为 "YYYY-MM"，income/expense/balance 单位均为分。
nlohmann::json to_json(const MonthlyIncomeExpense& amount) {
  return {{"month", amount.month},
          {"income", amount.income},
          {"expense", amount.expense},
          {"balance", amount.balance()}};
}

// 家庭总览：总资产 / 总负债 / 净资产 / 当月收入 / 支出 / 结余，
// 以及按成员、按账户、按资产类型的聚合数组。
nlohmann::json to_json(const HouseholdOverview& overview) {
  nlohmann::json by_member = nlohmann::json::array();
  for (const auto& item : overview.by_member) {
    by_member.push_back(to_json(item));
  }
  nlohmann::json by_account = nlohmann::json::array();
  for (const auto& item : overview.by_account) {
    by_account.push_back(to_json(item));
  }
  nlohmann::json by_type = nlohmann::json::array();
  for (const auto& item : overview.by_type) {
    by_type.push_back(to_json(item));
  }
  return {{"total_assets", overview.total_assets},
          {"total_liabilities", overview.total_liabilities},
          {"net_worth", overview.net_worth},
          {"month_income", overview.month_income},
          {"month_expense", overview.month_expense},
          {"month_balance", overview.month_balance},
          {"by_member", by_member},
          {"by_account", by_account},
          {"by_type", by_type}};
}

// 区间统计：from/to 时间范围、收入 / 支出 / 结余，以及按成员、
// 收入分类、支出分类的聚合数组。
nlohmann::json to_json(const PeriodStatistics& statistics) {
  nlohmann::json by_member = nlohmann::json::array();
  for (const auto& item : statistics.by_member) {
    by_member.push_back(to_json(item));
  }
  nlohmann::json income_categories = nlohmann::json::array();
  for (const auto& item : statistics.income_categories) {
    income_categories.push_back(to_json(item));
  }
  nlohmann::json expense_categories = nlohmann::json::array();
  for (const auto& item : statistics.expense_categories) {
    expense_categories.push_back(to_json(item));
  }
  return {{"from", statistics.from_time},
          {"to", statistics.to_time},
          {"income", statistics.income},
          {"expense", statistics.expense},
          {"balance", statistics.balance},
          {"by_member", by_member},
          {"income_categories", income_categories},
          {"expense_categories", expense_categories}};
}

}  // namespace wt::dto

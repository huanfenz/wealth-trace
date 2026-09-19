#include "dto/serialization.hpp"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace wt::dto {
namespace {

nlohmann::json optional_text(const std::optional<std::string>& value) {
  return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

nlohmann::json optional_int(const std::optional<std::int64_t>& value) {
  return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

}  // namespace

nlohmann::json to_json(const Household& household) {
  return {{"id", household.id},
          {"name", household.name},
          {"created_at", household.created_at},
          {"updated_at", household.updated_at}};
}

nlohmann::json to_json(const HouseholdMember& member) {
  return {{"id", member.id},
          {"household_id", member.household_id},
          {"name", member.name},
          {"role", std::string(to_string(member.role))},
          {"status", std::string(to_string(member.status))},
          {"created_at", member.created_at},
          {"updated_at", member.updated_at}};
}

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

nlohmann::json to_json(const AccountView& view) {
  auto json = to_json(view.account);
  json["balance"] = view.balance;
  json["asset_count"] = view.asset_count;
  return json;
}

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

nlohmann::json to_json(const TermDepositDetail& detail) {
  nlohmann::json json;
  json["asset_id"] = detail.asset_id;
  json["principal"] = detail.principal;
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
  return json;
}

nlohmann::json to_json(const FundDetail& detail) {
  return {{"asset_id", detail.asset_id},
          {"fund_code", optional_text(detail.fund_code)},
          {"fund_name", optional_text(detail.fund_name)},
          {"fund_type", optional_text(detail.fund_type)},
          {"lock_start_date", optional_text(detail.lock_start_date)},
          {"lock_end_date", optional_text(detail.lock_end_date)}};
}

nlohmann::json to_json(const BondDetail& detail) {
  return {{"asset_id", detail.asset_id},
          {"bond_code", optional_text(detail.bond_code)},
          {"bond_name", optional_text(detail.bond_name)},
          {"principal", detail.principal},
          {"annual_coupon_rate", detail.annual_coupon_rate},
          {"purchase_date", optional_text(detail.purchase_date)},
          {"maturity_date", optional_text(detail.maturity_date)},
          {"lock_end_date", optional_text(detail.lock_end_date)}};
}

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

nlohmann::json to_json(const AssetBundle& bundle) {
  auto json = to_json(bundle.asset);
  json["term_deposit"] = bundle.term_deposit.has_value()
                             ? to_json(*bundle.term_deposit)
                             : nlohmann::json(nullptr);
  json["fund"] = bundle.fund.has_value() ? to_json(*bundle.fund) : nlohmann::json(nullptr);
  json["bond"] = bundle.bond.has_value() ? to_json(*bundle.bond) : nlohmann::json(nullptr);
  json["insurance"] = bundle.insurance.has_value() ? to_json(*bundle.insurance)
                                                    : nlohmann::json(nullptr);
  return json;
}

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

nlohmann::json to_json(const TransferResult& result) {
  return {{"outgoing", to_json(result.outgoing)}, {"incoming", to_json(result.incoming)}};
}

nlohmann::json to_json(const NamedAmount& amount) {
  return {{"id", amount.id}, {"name", amount.name}, {"amount", amount.amount}};
}

nlohmann::json to_json(const TypeAmount& amount) {
  return {{"asset_type", std::string(to_string(amount.type))}, {"amount", amount.amount}};
}

nlohmann::json to_json(const CategoryAmount& amount) {
  return {{"category", amount.category}, {"amount", amount.amount}};
}

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

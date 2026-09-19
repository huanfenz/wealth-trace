#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "model/enums.hpp"

namespace wt {

struct Household {
  std::int64_t id = 0;
  std::string name;
  std::string created_at;
  std::string updated_at;
};

struct HouseholdMember {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  std::string name;
  MemberRole role = MemberRole::Member;
  MemberStatus status = MemberStatus::Active;
  std::string created_at;
  std::string updated_at;
};

struct Account {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  std::int64_t owner_member_id = 0;
  std::string name;
  AccountType type = AccountType::Bank;
  std::optional<std::string> institution_name;
  std::optional<std::string> account_no_masked;
  std::optional<std::string> remark;
  bool enabled = true;
  std::string created_at;
  std::string updated_at;
};

struct Asset {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  std::int64_t owner_member_id = 0;
  std::int64_t account_id = 0;
  std::string name;
  AssetType asset_type = AssetType::Cash;
  std::int64_t opening_balance = 0;
  std::int64_t current_balance = 0;
  AssetStatus status = AssetStatus::Active;
  std::optional<std::string> remark;
  std::string created_at;
  std::string updated_at;
};

struct TermDepositDetail {
  std::int64_t asset_id = 0;
  std::int64_t principal = 0;
  std::int64_t annual_interest_rate = 0;  // scaled by 1e6
  std::optional<std::string> start_date;
  std::optional<std::string> maturity_date;
  std::optional<std::int64_t> term_value;
  std::optional<TermUnit> term_unit;
  std::optional<std::string> interest_type;
  bool auto_rollover = false;
  std::optional<std::string> maturity_action;
};

struct FundDetail {
  std::int64_t asset_id = 0;
  std::optional<std::string> fund_code;
  std::optional<std::string> fund_name;
  std::optional<std::string> fund_type;
  std::optional<std::string> lock_start_date;
  std::optional<std::string> lock_end_date;
};

struct BondDetail {
  std::int64_t asset_id = 0;
  std::optional<std::string> bond_code;
  std::optional<std::string> bond_name;
  std::int64_t principal = 0;
  std::int64_t annual_coupon_rate = 0;  // scaled by 1e6
  std::optional<std::string> purchase_date;
  std::optional<std::string> maturity_date;
  std::optional<std::string> lock_end_date;
};

struct InsuranceDetail {
  std::int64_t asset_id = 0;
  std::optional<std::string> policy_no;
  std::optional<std::string> insurance_company;
  std::optional<std::string> product_name;
  std::optional<std::string> insurance_type;
  std::optional<std::string> effective_date;
  std::optional<std::string> maturity_date;
  std::int64_t annual_premium = 0;
  std::int64_t total_paid_premium = 0;
  std::int64_t insured_amount = 0;
  std::optional<std::int64_t> payment_years;
};

struct Transaction {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  std::int64_t owner_member_id = 0;
  std::int64_t asset_id = 0;
  TransactionType type = TransactionType::Expense;
  std::optional<std::string> category;
  std::int64_t amount = 0;
  std::optional<std::int64_t> transfer_group_id;
  std::optional<std::int64_t> balance_before;
  std::optional<std::int64_t> balance_after;
  std::string transaction_time;
  std::optional<std::string> remark;
  TransactionStatus status = TransactionStatus::Normal;
  std::string created_at;
  std::string updated_at;
};

}  // namespace wt

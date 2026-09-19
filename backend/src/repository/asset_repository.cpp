#include "repository/asset_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

Asset map_asset(Statement& statement) {
  Asset asset;
  asset.id = statement.get_int64(0);
  asset.household_id = statement.get_int64(1);
  asset.owner_member_id = statement.get_int64(2);
  asset.account_id = statement.get_int64(3);
  asset.name = statement.get_text(4);
  asset.asset_type = parse_asset_type(statement.get_text(5)).value_or(AssetType::Other);
  asset.opening_balance = statement.get_int64(6);
  asset.current_balance = statement.get_int64(7);
  asset.status = parse_asset_status(statement.get_text(8)).value_or(AssetStatus::Active);
  asset.remark = statement.get_optional_text(9);
  asset.created_at = statement.get_text(10);
  asset.updated_at = statement.get_text(11);
  return asset;
}

constexpr const char* kSelectColumns =
    "id, household_id, owner_member_id, account_id, name, asset_type, "
    "opening_balance, current_balance, status, remark, created_at, updated_at";

}  // namespace

std::int64_t AssetRepository::create(const Asset& asset) {
  Statement statement(
      database_,
      "INSERT INTO asset (household_id, owner_member_id, account_id, name, asset_type, "
      "opening_balance, current_balance, status, remark, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  statement.bind(1, asset.household_id)
      .bind(2, asset.owner_member_id)
      .bind(3, asset.account_id)
      .bind(4, asset.name)
      .bind(5, std::string(to_string(asset.asset_type)))
      .bind(6, asset.opening_balance)
      .bind(7, asset.current_balance)
      .bind(8, std::string(to_string(asset.status)))
      .bind_optional_text(9, asset.remark)
      .bind(10, asset.created_at)
      .bind(11, asset.updated_at)
      .run();
  return database_.last_insert_rowid();
}

std::optional<Asset> AssetRepository::find_by_id(std::int64_t id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM asset WHERE id = ?;");
  statement.bind(1, id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_asset(statement);
}

std::vector<Asset> AssetRepository::list_by_household(
    std::int64_t household_id, std::optional<std::int64_t> owner_member_id,
    std::optional<std::int64_t> account_id) {
  std::string sql = std::string("SELECT ") + kSelectColumns +
                    " FROM asset WHERE household_id = ?";
  if (owner_member_id.has_value()) {
    sql += " AND owner_member_id = ?";
  }
  if (account_id.has_value()) {
    sql += " AND account_id = ?";
  }
  sql += " ORDER BY id ASC;";

  Statement statement(database_, sql);
  int index = 1;
  statement.bind(index++, household_id);
  if (owner_member_id.has_value()) {
    statement.bind(index++, *owner_member_id);
  }
  if (account_id.has_value()) {
    statement.bind(index++, *account_id);
  }

  std::vector<Asset> assets;
  while (statement.step()) {
    assets.push_back(map_asset(statement));
  }
  return assets;
}

bool AssetRepository::update_metadata(const Asset& asset) {
  Statement statement(database_,
                      "UPDATE asset SET name = ?, asset_type = ?, "
                      "opening_balance = ?, remark = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, asset.name)
      .bind(2, std::string(to_string(asset.asset_type)))
      .bind(3, asset.opening_balance)
      .bind_optional_text(4, asset.remark)
      .bind(5, asset.updated_at)
      .bind(6, asset.id)
      .run();
  return database_.changes() > 0;
}

bool AssetRepository::update_balance(std::int64_t id, std::int64_t current_balance,
                                     const std::string& updated_at) {
  Statement statement(database_,
                      "UPDATE asset SET current_balance = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, current_balance).bind(2, updated_at).bind(3, id).run();
  return database_.changes() > 0;
}

bool AssetRepository::update_status(std::int64_t id, AssetStatus status,
                                    const std::string& updated_at) {
  Statement statement(database_,
                      "UPDATE asset SET status = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, std::string(to_string(status))).bind(2, updated_at).bind(3, id).run();
  return database_.changes() > 0;
}

bool AssetRepository::exists(std::int64_t id) {
  Statement statement(database_, "SELECT COUNT(*) FROM asset WHERE id = ?;");
  statement.bind(1, id);
  if (statement.step()) {
    return statement.get_int64(0) > 0;
  }
  return false;
}

std::int64_t AssetRepository::count_by_account(std::int64_t account_id) {
  Statement statement(database_, "SELECT COUNT(*) FROM asset WHERE account_id = ?;");
  statement.bind(1, account_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

std::int64_t AssetRepository::sum_balance_by_account(std::int64_t account_id) {
  Statement statement(
      database_,
      "SELECT COALESCE(SUM(current_balance), 0) FROM asset "
      "WHERE account_id = ? AND status = 'ACTIVE';");
  statement.bind(1, account_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// ---------------------------------------------------------------------------
// term_deposit_detail
// ---------------------------------------------------------------------------
void AssetRepository::upsert_term_deposit_detail(const TermDepositDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO term_deposit_detail (asset_id, principal, annual_interest_rate, "
      "start_date, maturity_date, term_value, term_unit, interest_type, auto_rollover, "
      "maturity_action) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET principal = excluded.principal, "
      "annual_interest_rate = excluded.annual_interest_rate, start_date = excluded.start_date, "
      "maturity_date = excluded.maturity_date, term_value = excluded.term_value, "
      "term_unit = excluded.term_unit, interest_type = excluded.interest_type, "
      "auto_rollover = excluded.auto_rollover, maturity_action = excluded.maturity_action;");
  statement.bind(1, detail.asset_id)
      .bind(2, detail.principal)
      .bind(3, detail.annual_interest_rate)
      .bind_optional_text(4, detail.start_date)
      .bind_optional_text(5, detail.maturity_date)
      .bind_optional_int64(6, detail.term_value)
      .bind_optional_text(7, detail.term_unit.has_value()
                                 ? std::optional<std::string>(
                                       std::string(to_string(*detail.term_unit)))
                                 : std::nullopt)
      .bind_optional_text(8, detail.interest_type)
      .bind(9, detail.auto_rollover)
      .bind_optional_text(10, detail.maturity_action)
      .run();
}

std::optional<TermDepositDetail> AssetRepository::find_term_deposit_detail(
    std::int64_t asset_id) {
  Statement statement(
      database_,
      "SELECT asset_id, principal, annual_interest_rate, start_date, maturity_date, "
      "term_value, term_unit, interest_type, auto_rollover, maturity_action "
      "FROM term_deposit_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  TermDepositDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.principal = statement.get_int64(1);
  detail.annual_interest_rate = statement.get_int64(2);
  detail.start_date = statement.get_optional_text(3);
  detail.maturity_date = statement.get_optional_text(4);
  detail.term_value = statement.get_optional_int64(5);
  if (const auto unit = statement.get_optional_text(6); unit.has_value()) {
    detail.term_unit = parse_term_unit(*unit);
  }
  detail.interest_type = statement.get_optional_text(7);
  detail.auto_rollover = statement.get_bool(8);
  detail.maturity_action = statement.get_optional_text(9);
  return detail;
}

void AssetRepository::delete_term_deposit_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM term_deposit_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// ---------------------------------------------------------------------------
// fund_detail
// ---------------------------------------------------------------------------
void AssetRepository::upsert_fund_detail(const FundDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO fund_detail (asset_id, fund_code, fund_name, fund_type, "
      "lock_start_date, lock_end_date) VALUES (?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET fund_code = excluded.fund_code, "
      "fund_name = excluded.fund_name, fund_type = excluded.fund_type, "
      "lock_start_date = excluded.lock_start_date, lock_end_date = excluded.lock_end_date;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.fund_code)
      .bind_optional_text(3, detail.fund_name)
      .bind_optional_text(4, detail.fund_type)
      .bind_optional_text(5, detail.lock_start_date)
      .bind_optional_text(6, detail.lock_end_date)
      .run();
}

std::optional<FundDetail> AssetRepository::find_fund_detail(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT asset_id, fund_code, fund_name, fund_type, lock_start_date, "
                      "lock_end_date FROM fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  FundDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.fund_code = statement.get_optional_text(1);
  detail.fund_name = statement.get_optional_text(2);
  detail.fund_type = statement.get_optional_text(3);
  detail.lock_start_date = statement.get_optional_text(4);
  detail.lock_end_date = statement.get_optional_text(5);
  return detail;
}

void AssetRepository::delete_fund_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// ---------------------------------------------------------------------------
// bond_detail
// ---------------------------------------------------------------------------
void AssetRepository::upsert_bond_detail(const BondDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO bond_detail (asset_id, bond_code, bond_name, principal, "
      "annual_coupon_rate, purchase_date, maturity_date, lock_end_date) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET bond_code = excluded.bond_code, "
      "bond_name = excluded.bond_name, principal = excluded.principal, "
      "annual_coupon_rate = excluded.annual_coupon_rate, purchase_date = excluded.purchase_date, "
      "maturity_date = excluded.maturity_date, lock_end_date = excluded.lock_end_date;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.bond_code)
      .bind_optional_text(3, detail.bond_name)
      .bind(4, detail.principal)
      .bind(5, detail.annual_coupon_rate)
      .bind_optional_text(6, detail.purchase_date)
      .bind_optional_text(7, detail.maturity_date)
      .bind_optional_text(8, detail.lock_end_date)
      .run();
}

std::optional<BondDetail> AssetRepository::find_bond_detail(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT asset_id, bond_code, bond_name, principal, annual_coupon_rate, "
                      "purchase_date, maturity_date, lock_end_date FROM bond_detail "
                      "WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  BondDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.bond_code = statement.get_optional_text(1);
  detail.bond_name = statement.get_optional_text(2);
  detail.principal = statement.get_int64(3);
  detail.annual_coupon_rate = statement.get_int64(4);
  detail.purchase_date = statement.get_optional_text(5);
  detail.maturity_date = statement.get_optional_text(6);
  detail.lock_end_date = statement.get_optional_text(7);
  return detail;
}

void AssetRepository::delete_bond_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM bond_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// ---------------------------------------------------------------------------
// insurance_detail
// ---------------------------------------------------------------------------
void AssetRepository::upsert_insurance_detail(const InsuranceDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO insurance_detail (asset_id, policy_no, insurance_company, product_name, "
      "insurance_type, effective_date, maturity_date, annual_premium, total_paid_premium, "
      "insured_amount, payment_years) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET policy_no = excluded.policy_no, "
      "insurance_company = excluded.insurance_company, product_name = excluded.product_name, "
      "insurance_type = excluded.insurance_type, effective_date = excluded.effective_date, "
      "maturity_date = excluded.maturity_date, annual_premium = excluded.annual_premium, "
      "total_paid_premium = excluded.total_paid_premium, insured_amount = excluded.insured_amount, "
      "payment_years = excluded.payment_years;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.policy_no)
      .bind_optional_text(3, detail.insurance_company)
      .bind_optional_text(4, detail.product_name)
      .bind_optional_text(5, detail.insurance_type)
      .bind_optional_text(6, detail.effective_date)
      .bind_optional_text(7, detail.maturity_date)
      .bind(8, detail.annual_premium)
      .bind(9, detail.total_paid_premium)
      .bind(10, detail.insured_amount)
      .bind_optional_int64(11, detail.payment_years)
      .run();
}

std::optional<InsuranceDetail> AssetRepository::find_insurance_detail(
    std::int64_t asset_id) {
  Statement statement(
      database_,
      "SELECT asset_id, policy_no, insurance_company, product_name, insurance_type, "
      "effective_date, maturity_date, annual_premium, total_paid_premium, insured_amount, "
      "payment_years FROM insurance_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  InsuranceDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.policy_no = statement.get_optional_text(1);
  detail.insurance_company = statement.get_optional_text(2);
  detail.product_name = statement.get_optional_text(3);
  detail.insurance_type = statement.get_optional_text(4);
  detail.effective_date = statement.get_optional_text(5);
  detail.maturity_date = statement.get_optional_text(6);
  detail.annual_premium = statement.get_int64(7);
  detail.total_paid_premium = statement.get_int64(8);
  detail.insured_amount = statement.get_int64(9);
  detail.payment_years = statement.get_optional_int64(10);
  return detail;
}

void AssetRepository::delete_insurance_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM insurance_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

}  // namespace wt

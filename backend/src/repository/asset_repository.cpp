// asset_repository.cpp：asset 表及四张 1:0..1 明细表的 SQL 实现与行映射。
#include "repository/asset_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

// 行映射：列下标必须与 kSelectColumns 的顺序严格一致。
// 0=id 1=household_id 2=owner_member_id 3=account_id 4=name 5=asset_type
// 6=opening_balance 7=current_balance 8=status 9=remark 10=created_at 11=updated_at
// 金额均为整数分；负债的 current_balance 为负数。枚举解析失败回退默认值。
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

// SELECT 列顺序，与 map_asset 的下标一一对应。
constexpr const char* kSelectColumns =
    "id, household_id, owner_member_id, account_id, name, asset_type, "
    "opening_balance, current_balance, status, remark, created_at, updated_at";

// term_deposit_detail 的 SELECT 列顺序；与 map_term_deposit 的列下标严格对应。
constexpr const char* kTermDepositColumns =
    "asset_id, annual_interest_rate, start_date, maturity_date, "
    "term_value, term_unit, interest_type, auto_rollover, maturity_action";

// 行映射：0=asset_id 1=annual_interest_rate 2=start_date 3=maturity_date
// 4=term_value 5=term_unit 6=interest_type 7=auto_rollover 8=maturity_action
TermDepositDetail map_term_deposit(Statement& statement) {
  TermDepositDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.annual_interest_rate = statement.get_int64(1);
  detail.start_date = statement.get_optional_text(2);
  detail.maturity_date = statement.get_optional_text(3);
  detail.term_value = statement.get_optional_int64(4);
  if (const auto unit = statement.get_optional_text(5); unit.has_value()) {
    detail.term_unit = parse_term_unit(*unit);
  }
  detail.interest_type = statement.get_optional_text(6);
  detail.auto_rollover = statement.get_bool(7);
  detail.maturity_action = statement.get_optional_text(8);
  return detail;
}

}  // namespace

// 插入资产（冗余 household_id/owner_member_id 直接写入），返回自增主键。
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

// 动态查询：household_id 为必选；owner_member_id、account_id 有值时才依次追加
// 对应的 AND 条件，并用自增 index 保证占位符顺序与绑定顺序一致。
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

// 只更新元数据，不触碰 current_balance/status/account_id。
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

// 只改当前价值（分），用于交易后回写余额。
bool AssetRepository::update_balance(std::int64_t id, std::int64_t current_balance,
                                     const std::string& updated_at) {
  Statement statement(database_,
                      "UPDATE asset SET current_balance = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, current_balance).bind(2, updated_at).bind(3, id).run();
  return database_.changes() > 0;
}

// 只改资产状态（ACTIVE 等），如销户/停用。
bool AssetRepository::update_status(std::int64_t id, AssetStatus status,
                                    const std::string& updated_at) {
  Statement statement(database_,
                      "UPDATE asset SET status = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, std::string(to_string(status))).bind(2, updated_at).bind(3, id).run();
  return database_.changes() > 0;
}

bool AssetRepository::remove(std::int64_t id) {
  // 明细表与 "transaction" 的 asset_id 外键均为 ON DELETE CASCADE，
  // 删除资产行时数据库会自动级联清理其明细与全部流水。
  Statement statement(database_, "DELETE FROM asset WHERE id = ?;");
  statement.bind(1, id);
  statement.run();
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

// 仅汇总 status='ACTIVE' 的资产；COALESCE 保证无匹配行时返回 0 而非 NULL。
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
// upsert 语义：以 asset_id 为唯一键，冲突时用 excluded 值整行覆盖更新，
// 保证与 asset 的 1:0..1 关系（存在则更新，不存在则插入）。
void AssetRepository::upsert_term_deposit_detail(const TermDepositDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO term_deposit_detail (asset_id, annual_interest_rate, "
      "start_date, maturity_date, term_value, term_unit, interest_type, auto_rollover, "
      "maturity_action) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET "
      "annual_interest_rate = excluded.annual_interest_rate, start_date = excluded.start_date, "
      "maturity_date = excluded.maturity_date, term_value = excluded.term_value, "
      "term_unit = excluded.term_unit, interest_type = excluded.interest_type, "
      "auto_rollover = excluded.auto_rollover, maturity_action = excluded.maturity_action;");
  statement.bind(1, detail.asset_id)
      .bind(2, detail.annual_interest_rate)
      .bind_optional_text(3, detail.start_date)
      .bind_optional_text(4, detail.maturity_date)
      .bind_optional_int64(5, detail.term_value)
      .bind_optional_text(6, detail.term_unit.has_value()
                                 ? std::optional<std::string>(
                                       std::string(to_string(*detail.term_unit)))
                                 : std::nullopt)
      .bind_optional_text(7, detail.interest_type)
      .bind(8, detail.auto_rollover)
      .bind_optional_text(9, detail.maturity_action)
      .run();
}

std::optional<TermDepositDetail> AssetRepository::find_term_deposit_detail(
    std::int64_t asset_id) {
  Statement statement(database_, std::string("SELECT ") + kTermDepositColumns +
                                     " FROM term_deposit_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_term_deposit(statement);
}

void AssetRepository::delete_term_deposit_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM term_deposit_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// 只取 ACTIVE 资产下的自动续存存款；已关闭（CLOSED）资产无需推进。
std::vector<TermDepositDetail> AssetRepository::list_auto_rollover_term_deposits() {
  Statement statement(
      database_,
      std::string("SELECT ") + kTermDepositColumns +
          " FROM term_deposit_detail d JOIN asset a ON a.id = d.asset_id "
          "WHERE d.auto_rollover = 1 AND a.status = 'ACTIVE' ORDER BY d.asset_id ASC;");
  std::vector<TermDepositDetail> deposits;
  while (statement.step()) {
    deposits.push_back(map_term_deposit(statement));
  }
  return deposits;
}

bool AssetRepository::update_term_deposit_period(std::int64_t asset_id,
                                                 const std::string& start_date,
                                                 const std::string& maturity_date) {
  Statement statement(
      database_,
      "UPDATE term_deposit_detail SET start_date = ?, maturity_date = ? WHERE asset_id = ?;");
  statement.bind(1, start_date).bind(2, maturity_date).bind(3, asset_id).run();
  return database_.changes() > 0;
}

// ---------------------------------------------------------------------------
// fund_detail
// ---------------------------------------------------------------------------
// upsert 语义同 term_deposit_detail：ON CONFLICT(asset_id) DO UPDATE 整行覆盖。
void AssetRepository::upsert_fund_detail(const FundDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO fund_detail (asset_id, fund_code, fund_type, "
      "lock_start_date, lock_end_date) VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET fund_code = excluded.fund_code, "
      "fund_type = excluded.fund_type, "
      "lock_start_date = excluded.lock_start_date, lock_end_date = excluded.lock_end_date;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.fund_code)
      .bind_optional_text(3, detail.fund_type)
      .bind_optional_text(4, detail.lock_start_date)
      .bind_optional_text(5, detail.lock_end_date)
      .run();
}

std::optional<FundDetail> AssetRepository::find_fund_detail(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT asset_id, fund_code, fund_type, lock_start_date, "
                      "lock_end_date FROM fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  FundDetail detail;
  // 列下标：0=asset_id 1=fund_code 2=fund_type 3=lock_start_date 4=lock_end_date
  detail.asset_id = statement.get_int64(0);
  detail.fund_code = statement.get_optional_text(1);
  detail.fund_type = statement.get_optional_text(2);
  detail.lock_start_date = statement.get_optional_text(3);
  detail.lock_end_date = statement.get_optional_text(4);
  return detail;
}

void AssetRepository::delete_fund_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// ---------------------------------------------------------------------------
// bond_detail
// ---------------------------------------------------------------------------
// upsert 语义同上：ON CONFLICT(asset_id) DO UPDATE 整行覆盖。
void AssetRepository::upsert_bond_detail(const BondDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO bond_detail (asset_id, bond_code, "
      "annual_coupon_rate, purchase_date, maturity_date, lock_end_date) "
      "VALUES (?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET bond_code = excluded.bond_code, "
      "annual_coupon_rate = excluded.annual_coupon_rate, purchase_date = excluded.purchase_date, "
      "maturity_date = excluded.maturity_date, lock_end_date = excluded.lock_end_date;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.bond_code)
      .bind(3, detail.annual_coupon_rate)
      .bind_optional_text(4, detail.purchase_date)
      .bind_optional_text(5, detail.maturity_date)
      .bind_optional_text(6, detail.lock_end_date)
      .run();
}

std::optional<BondDetail> AssetRepository::find_bond_detail(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT asset_id, bond_code, annual_coupon_rate, "
                      "purchase_date, maturity_date, lock_end_date FROM bond_detail "
                      "WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  BondDetail detail;
  // 列下标：0=asset_id 1=bond_code 2=annual_coupon_rate
  // 3=purchase_date 4=maturity_date 5=lock_end_date
  detail.asset_id = statement.get_int64(0);
  detail.bond_code = statement.get_optional_text(1);
  detail.annual_coupon_rate = statement.get_int64(2);
  detail.purchase_date = statement.get_optional_text(3);
  detail.maturity_date = statement.get_optional_text(4);
  detail.lock_end_date = statement.get_optional_text(5);
  return detail;
}

void AssetRepository::delete_bond_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM bond_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

// ---------------------------------------------------------------------------
// bond_fund_detail
// ---------------------------------------------------------------------------
namespace {

// bond_fund_detail 的 SELECT 列顺序；与 map_bond_fund 的列下标严格对应。
constexpr const char* kBondFundSelectColumns =
    "asset_id, fund_code, expected_annual_yield_rate, "
    "purchase_date, holding_mode, holding_period_days, first_redeem_date, "
    "next_redeem_date, maturity_date";

// 行映射：0=asset_id 1=fund_code 2=expected_annual_yield_rate 3=purchase_date
// 4=holding_mode 5=holding_period_days 6=first_redeem_date 7=next_redeem_date
// 8=maturity_date
BondFundDetail map_bond_fund(Statement& statement) {
  BondFundDetail detail;
  detail.asset_id = statement.get_int64(0);
  detail.fund_code = statement.get_optional_text(1);
  detail.expected_annual_yield_rate = statement.get_optional_int64(2);
  detail.purchase_date = statement.get_text(3);
  detail.holding_mode =
      parse_holding_mode(statement.get_text(4)).value_or(HoldingMode::MinHolding);
  detail.holding_period_days = statement.get_int64(5);
  detail.first_redeem_date = statement.get_optional_text(6);
  detail.next_redeem_date = statement.get_optional_text(7);
  detail.maturity_date = statement.get_optional_text(8);
  return detail;
}

}  // namespace

// upsert 语义同其它明细表：ON CONFLICT(asset_id) DO UPDATE 整行覆盖。
void AssetRepository::upsert_bond_fund_detail(const BondFundDetail& detail) {
  Statement statement(
      database_,
      "INSERT INTO bond_fund_detail (asset_id, fund_code, "
      "expected_annual_yield_rate, purchase_date, holding_mode, holding_period_days, "
      "first_redeem_date, next_redeem_date, maturity_date) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(asset_id) DO UPDATE SET fund_code = excluded.fund_code, "
      "expected_annual_yield_rate = excluded.expected_annual_yield_rate, "
      "purchase_date = excluded.purchase_date, holding_mode = excluded.holding_mode, "
      "holding_period_days = excluded.holding_period_days, "
      "first_redeem_date = excluded.first_redeem_date, "
      "next_redeem_date = excluded.next_redeem_date, maturity_date = excluded.maturity_date;");
  statement.bind(1, detail.asset_id)
      .bind_optional_text(2, detail.fund_code)
      .bind_optional_int64(3, detail.expected_annual_yield_rate)
      .bind(4, detail.purchase_date)
      .bind(5, std::string(to_string(detail.holding_mode)))
      .bind(6, detail.holding_period_days)
      .bind_optional_text(7, detail.first_redeem_date)
      .bind_optional_text(8, detail.next_redeem_date)
      .bind_optional_text(9, detail.maturity_date)
      .run();
}

std::optional<BondFundDetail> AssetRepository::find_bond_fund_detail(std::int64_t asset_id) {
  Statement statement(database_, std::string("SELECT ") + kBondFundSelectColumns +
                                     " FROM bond_fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_bond_fund(statement);
}

void AssetRepository::delete_bond_fund_detail(std::int64_t asset_id) {
  Statement statement(database_, "DELETE FROM bond_fund_detail WHERE asset_id = ?;");
  statement.bind(1, asset_id).run();
}

std::vector<BondFundDetail> AssetRepository::list_rolling_bond_funds() {
  Statement statement(database_, std::string("SELECT ") + kBondFundSelectColumns +
                                     " FROM bond_fund_detail WHERE holding_mode = 'ROLLING' "
                                     "ORDER BY asset_id ASC;");
  std::vector<BondFundDetail> funds;
  while (statement.step()) {
    funds.push_back(map_bond_fund(statement));
  }
  return funds;
}

bool AssetRepository::update_bond_fund_next_redeem_date(std::int64_t asset_id,
                                                        const std::string& next_redeem_date) {
  Statement statement(
      database_,
      "UPDATE bond_fund_detail SET next_redeem_date = ? WHERE asset_id = ?;");
  statement.bind(1, next_redeem_date).bind(2, asset_id).run();
  return database_.changes() > 0;
}

// ---------------------------------------------------------------------------
// insurance_detail
// ---------------------------------------------------------------------------
// upsert 语义同上：ON CONFLICT(asset_id) DO UPDATE 整行覆盖。
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
  // 列下标：0=asset_id 1=policy_no 2=insurance_company 3=product_name
  // 4=insurance_type 5=effective_date 6=maturity_date 7=annual_premium
  // 8=total_paid_premium 9=insured_amount 10=payment_years
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

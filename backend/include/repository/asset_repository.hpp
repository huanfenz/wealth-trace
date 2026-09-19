#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

class AssetRepository {
 public:
  explicit AssetRepository(Database& database) : database_(database) {}

  std::int64_t create(const Asset& asset);
  std::optional<Asset> find_by_id(std::int64_t id);
  std::vector<Asset> list_by_household(std::int64_t household_id,
                                       std::optional<std::int64_t> owner_member_id,
                                       std::optional<std::int64_t> account_id);

  bool update_metadata(const Asset& asset);
  bool update_balance(std::int64_t id, std::int64_t current_balance,
                      const std::string& updated_at);
  bool update_status(std::int64_t id, AssetStatus status, const std::string& updated_at);
  bool exists(std::int64_t id);
  std::int64_t count_by_account(std::int64_t account_id);
  std::int64_t sum_balance_by_account(std::int64_t account_id);

  // Detail tables (1:0..1). `upsert_*` inserts or replaces the detail row.
  void upsert_term_deposit_detail(const TermDepositDetail& detail);
  std::optional<TermDepositDetail> find_term_deposit_detail(std::int64_t asset_id);
  void delete_term_deposit_detail(std::int64_t asset_id);

  void upsert_fund_detail(const FundDetail& detail);
  std::optional<FundDetail> find_fund_detail(std::int64_t asset_id);
  void delete_fund_detail(std::int64_t asset_id);

  void upsert_bond_detail(const BondDetail& detail);
  std::optional<BondDetail> find_bond_detail(std::int64_t asset_id);
  void delete_bond_detail(std::int64_t asset_id);

  void upsert_insurance_detail(const InsuranceDetail& detail);
  std::optional<InsuranceDetail> find_insurance_detail(std::int64_t asset_id);
  void delete_insurance_detail(std::int64_t asset_id);

 private:
  Database& database_;
};

}  // namespace wt

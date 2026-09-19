// 资产（asset）及其明细表数据访问：封装 asset 与
// term_deposit/fund/bond/insurance_detail 的 SQL 执行与行映射。
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/entities.hpp"

namespace wt {

class Database;

// asset 表仓储。当前价值只存于 current_balance（负债为负数），
// household_id/owner_member_id 为冗余字段，来源 account.owner_member_id。
class AssetRepository {
 public:
  explicit AssetRepository(Database& database) : database_(database) {}

  // 插入一个资产，返回新行自增主键 id。
  std::int64_t create(const Asset& asset);
  // 按主键查询，不存在返回 std::nullopt。
  std::optional<Asset> find_by_id(std::int64_t id);
  // 查询某家庭下的资产；owner_member_id / account_id 有值时追加对应过滤条件。
  std::vector<Asset> list_by_household(std::int64_t household_id,
                                       std::optional<std::int64_t> owner_member_id,
                                       std::optional<std::int64_t> account_id);

  // 更新资产元数据（name/asset_type/opening_balance/remark），不动 current_balance。
  bool update_metadata(const Asset& asset);
  // 单独更新 current_balance（金额单位：分）与 updated_at。
  bool update_balance(std::int64_t id, std::int64_t current_balance,
                      const std::string& updated_at);
  // 单独更新资产状态（ACTIVE/...）与 updated_at。
  bool update_status(std::int64_t id, AssetStatus status, const std::string& updated_at);
  // 判断主键是否存在。
  bool exists(std::int64_t id);
  // 统计某账户下的资产数量。
  std::int64_t count_by_account(std::int64_t account_id);
  // 汇总某账户下 ACTIVE 资产的 current_balance 之和（无行时为 0）。
  std::int64_t sum_balance_by_account(std::int64_t account_id);

  // Detail tables (1:0..1). `upsert_*` inserts or replaces the detail row.
  // 明细表与 asset 是 1:0..1，upsert_* 以 asset_id 为冲突键，存在则整行覆盖。
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

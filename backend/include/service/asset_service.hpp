// 资产服务：资产（Asset）及其明细（定期/基金/债券/保险）的创建与维护。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/account_repository.hpp"
#include "repository/asset_repository.hpp"
#include "repository/transaction_repository.hpp"

namespace wt {

class Database;

// 创建资产的入参。金额单位为「分」；opening_balance 对普通资产须 >= 0，
// 对负债须 <= 0。四个明细块最多只能提供其一，且类型须与 asset_type 一致。
struct AssetCreateInput {
  std::int64_t account_id = 0;
  std::string name;
  AssetType asset_type = AssetType::Cash;
  std::int64_t opening_balance = 0;
  std::optional<std::string> remark;
  std::optional<TermDepositDetail> term_deposit;
  std::optional<FundDetail> fund;
  std::optional<BondDetail> bond;
  std::optional<BondFundDetail> bond_fund;
  std::optional<InsuranceDetail> insurance;
};

// 资产聚合结果：资产本体 + 至多一个明细块（由 asset_type 决定）。
struct AssetBundle {
  Asset asset;
  std::optional<TermDepositDetail> term_deposit;
  std::optional<FundDetail> fund;
  std::optional<BondDetail> bond;
  std::optional<BondFundDetail> bond_fund;
  std::optional<InsuranceDetail> insurance;
};

// 资产服务：资产是余额与交易的载体。冗余属主 household_id/owner_member_id
// 从所属账户复制而来。明细与资产本体一并读写，创建时用事务保证原子性。
class AssetService {
 public:
  explicit AssetService(Database& database)
      : accounts_(database), assets_(database), transactions_(database),
        database_(database) {}

  // 创建资产：账户不存在抛 not_found；校验 opening_balance 符号与明细组合，
  // 在事务中同时写入资产与（可选）明细，全成功或全回滚。
  // current_balance 初始化为 opening_balance（此时还没有任何交易）。
  AssetBundle create(std::int64_t household_id, const AssetCreateInput& input);

  // 获取资产及其明细聚合；资产不存在抛 not_found。
  AssetBundle get_bundle(std::int64_t id);

  // 按 id 获取资产本体；不存在抛 not_found。
  Asset get(std::int64_t id);

  // 按家庭/属主/账户（均可选）列出资产并附带明细。
  std::vector<AssetBundle> list_bundles(std::int64_t household_id,
                                        std::optional<std::int64_t> owner_member_id,
                                        std::optional<std::int64_t> account_id);

  // 更新资产名称、备注及（可选）opening_balance：
  // - 一旦有交易，禁止改 opening_balance（抛 conflict），否则账实无法对齐；
  // - 无交易时允许修改，并按差额同步 current_balance（保持
  //   current_balance = opening_balance + Σdelta）。两步在同一事务内完成。
  Asset update_metadata(std::int64_t id, const std::string& name,
                        std::optional<std::int64_t> opening_balance,
                        const std::optional<std::string>& remark);

  // 更新资产状态（ACTIVE/CLOSED）；资产不存在抛 not_found。
  // CLOSED 资产不计入统计且不能再产生交易（由交易服务校验）。
  Asset update_status(std::int64_t id, AssetStatus status);

  // 更新某类型的明细块：detail_type 必须等于 asset.asset_type（否则
  // invalid_request），对应明细指针必须非空，其余类型会报「无明细块」。
  AssetBundle update_detail(std::int64_t id, AssetType detail_type,
                            const TermDepositDetail* term_deposit,
                            const FundDetail* fund, const BondDetail* bond,
                            const BondFundDetail* bond_fund,
                            const InsuranceDetail* insurance);

  // 删除资产：资产不存在抛 not_found。资产下的全部流水与明细块会被级联删除，
  // 属不可恢复操作，调用方需二次确认。整个删除在同一事务内完成。
  void remove(std::int64_t id);

 private:
  // 按 asset_type 读取对应明细表，组装成 AssetBundle。
  AssetBundle load_bundle(const Asset& asset);
  // 校验一次最多一个明细块，且明细类型与 asset_type 一致。
  void validate_detail_combination(const Asset& asset,
                                   const AssetCreateInput& input) const;

  AccountRepository accounts_;
  AssetRepository assets_;
  TransactionRepository transactions_;
  Database& database_;
};

}  // namespace wt

// 账户服务：资金账户（Account）的增删改查，并聚合其下资产的余额视图。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/entities.hpp"
#include "repository/account_repository.hpp"
#include "repository/asset_repository.hpp"
#include "repository/household_repository.hpp"
#include "repository/member_repository.hpp"

namespace wt {

class Database;

// 账户视图：账户本体 + 其下所有资产余额之和（分）与资产数量。
// balance 由各资产 current_balance 汇总（负债为负，会自然抵减）。
struct AccountView {
  Account account;
  std::int64_t balance = 0;      // sum of active asset balances (minor units)
  std::int64_t asset_count = 0;  // number of assets under this account
};

// 账户服务：维护 Account。账户是资产/交易冗余属主（household_id、
// owner_member_id）的来源，因此一旦账户下有资产，禁止更改属主。
class AccountService {
 public:
  explicit AccountService(Database& database)
      : households_(database), members_(database), accounts_(database),
        assets_(database) {}

  // 新建账户：先校验家庭与属主成员存在且属于同一家庭（否则 not_found /
  // invalid_request），再校验 name 并清洗可选字段。返回带 id 的账户。
  Account create(std::int64_t household_id, std::int64_t owner_member_id,
                 const std::string& name, AccountType type,
                 const std::optional<std::string>& institution_name,
                 const std::optional<std::string>& account_no_masked,
                 const std::optional<std::string>& remark, bool enabled);

  // 列出某家庭账户，可按属主成员过滤；家庭不存在抛 not_found。
  std::vector<Account> list(std::int64_t household_id,
                            std::optional<std::int64_t> owner_member_id);

  // 在 list 基础上为每个账户补充余额与资产数量，供列表页直接展示。
  std::vector<AccountView> list_views(std::int64_t household_id,
                                      std::optional<std::int64_t> owner_member_id);

  // 获取单个账户的视图（含聚合余额）；账户不存在抛 not_found。
  AccountView get_view(std::int64_t id);

  // 按 id 获取账户；不存在抛 not_found。
  Account get(std::int64_t id);

  // 更新账户：若变更属主成员，仅当账户下没有资产时才允许（否则 conflict），
  // 因为资产/交易冗余保存了 owner_member_id；随后校验新成员属于本家庭。
  // 其余字段（名称/类型/可选信息/enabled）直接覆盖，成功刷新 updated_at。
  Account update(std::int64_t id, std::int64_t owner_member_id,
                 const std::string& name, AccountType type,
                 const std::optional<std::string>& institution_name,
                 const std::optional<std::string>& account_no_masked,
                 const std::optional<std::string>& remark, bool enabled);

 private:
  // 前置校验：家庭必须存在，否则抛 not_found。
  void require_household(std::int64_t household_id);
  // 前置校验：成员存在且 household_id 与入参一致，否则抛 not_found /
  // invalid_request，防止把账户挂到别家成员名下。
  void require_member(std::int64_t household_id, std::int64_t member_id);

  HouseholdRepository households_;
  MemberRepository members_;
  AccountRepository accounts_;
  AssetRepository assets_;
};

}  // namespace wt

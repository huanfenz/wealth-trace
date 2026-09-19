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

struct AccountView {
  Account account;
  std::int64_t balance = 0;      // sum of active asset balances (minor units)
  std::int64_t asset_count = 0;  // number of assets under this account
};

class AccountService {
 public:
  explicit AccountService(Database& database)
      : households_(database), members_(database), accounts_(database),
        assets_(database) {}

  Account create(std::int64_t household_id, std::int64_t owner_member_id,
                 const std::string& name, AccountType type,
                 const std::optional<std::string>& institution_name,
                 const std::optional<std::string>& account_no_masked,
                 const std::optional<std::string>& remark, bool enabled);
  std::vector<Account> list(std::int64_t household_id,
                            std::optional<std::int64_t> owner_member_id);
  std::vector<AccountView> list_views(std::int64_t household_id,
                                      std::optional<std::int64_t> owner_member_id);
  AccountView get_view(std::int64_t id);
  Account get(std::int64_t id);
  Account update(std::int64_t id, std::int64_t owner_member_id,
                 const std::string& name, AccountType type,
                 const std::optional<std::string>& institution_name,
                 const std::optional<std::string>& account_no_masked,
                 const std::optional<std::string>& remark, bool enabled);

 private:
  void require_household(std::int64_t household_id);
  void require_member(std::int64_t household_id, std::int64_t member_id);

  HouseholdRepository households_;
  MemberRepository members_;
  AccountRepository accounts_;
  AssetRepository assets_;
};

}  // namespace wt

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

struct AssetCreateInput {
  std::int64_t account_id = 0;
  std::string name;
  AssetType asset_type = AssetType::Cash;
  std::int64_t opening_balance = 0;
  std::optional<std::string> remark;
  std::optional<TermDepositDetail> term_deposit;
  std::optional<FundDetail> fund;
  std::optional<BondDetail> bond;
  std::optional<InsuranceDetail> insurance;
};

struct AssetBundle {
  Asset asset;
  std::optional<TermDepositDetail> term_deposit;
  std::optional<FundDetail> fund;
  std::optional<BondDetail> bond;
  std::optional<InsuranceDetail> insurance;
};

class AssetService {
 public:
  explicit AssetService(Database& database)
      : accounts_(database), assets_(database), transactions_(database),
        database_(database) {}

  AssetBundle create(const AssetCreateInput& input);
  AssetBundle get_bundle(std::int64_t id);
  Asset get(std::int64_t id);
  std::vector<AssetBundle> list_bundles(std::int64_t household_id,
                                        std::optional<std::int64_t> owner_member_id,
                                        std::optional<std::int64_t> account_id);

  Asset update_metadata(std::int64_t id, const std::string& name,
                        std::optional<std::int64_t> opening_balance,
                        const std::optional<std::string>& remark);
  Asset update_status(std::int64_t id, AssetStatus status);

  AssetBundle update_detail(std::int64_t id, AssetType detail_type,
                            const TermDepositDetail* term_deposit,
                            const FundDetail* fund, const BondDetail* bond,
                            const InsuranceDetail* insurance);

 private:
  AssetBundle load_bundle(const Asset& asset);
  void validate_detail_combination(const Asset& asset,
                                   const AssetCreateInput& input) const;

  AccountRepository accounts_;
  AssetRepository assets_;
  TransactionRepository transactions_;
  Database& database_;
};

}  // namespace wt

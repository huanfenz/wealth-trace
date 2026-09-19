#include "service/asset_service.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

std::optional<std::string> clean_optional(const std::optional<std::string>& value,
                                          std::string_view field,
                                          std::size_t max_length) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  const auto cleaned = strings::optional_text(*value, field, max_length);
  if (cleaned.empty()) {
    return std::nullopt;
  }
  return cleaned;
}

void validate_opening_balance(AssetType type, std::int64_t opening_balance) {
  if (type == AssetType::Liability && opening_balance > 0) {
    throw invalid_request("liability opening_balance must be zero or negative");
  }
  if (type != AssetType::Liability && opening_balance < 0) {
    throw invalid_request("asset opening_balance must be zero or positive");
  }
}

AssetType detail_type_of(const AssetCreateInput& input) {
  if (input.term_deposit.has_value()) return AssetType::TermDeposit;
  if (input.fund.has_value()) return AssetType::Fund;
  if (input.bond.has_value()) return AssetType::Bond;
  if (input.insurance.has_value()) return AssetType::Insurance;
  return input.asset_type;
}

int count_provided_details(const AssetCreateInput& input) {
  return (input.term_deposit.has_value() ? 1 : 0) + (input.fund.has_value() ? 1 : 0) +
         (input.bond.has_value() ? 1 : 0) + (input.insurance.has_value() ? 1 : 0);
}

}  // namespace

void AssetService::validate_detail_combination(const Asset& asset,
                                               const AssetCreateInput& input) const {
  if (count_provided_details(input) > 1) {
    throw invalid_request("only one detail block may be supplied per asset");
  }
  const AssetType detail_type = detail_type_of(input);
  if (count_provided_details(input) == 1 && detail_type != asset.asset_type) {
    throw invalid_request("detail block does not match asset_type");
  }
}

AssetBundle AssetService::create(const AssetCreateInput& input) {
  const auto account = accounts_.find_by_id(input.account_id);
  if (!account.has_value()) {
    throw not_found("account not found");
  }

  Asset asset;
  asset.household_id = account->household_id;
  asset.owner_member_id = account->owner_member_id;
  asset.account_id = account->id;
  asset.name = strings::require_text(input.name, "name", 100);
  asset.asset_type = input.asset_type;
  validate_opening_balance(asset.asset_type, input.opening_balance);
  asset.opening_balance = input.opening_balance;
  asset.current_balance = input.opening_balance;
  asset.status = AssetStatus::Active;
  asset.remark = clean_optional(input.remark, "remark", 500);
  asset.created_at = time_util::now_iso8601();
  asset.updated_at = asset.created_at;

  validate_detail_combination(asset, input);

  TransactionGuard transaction(database_);
  asset.id = assets_.create(asset);

  if (input.term_deposit.has_value()) {
    TermDepositDetail detail = *input.term_deposit;
    detail.asset_id = asset.id;
    assets_.upsert_term_deposit_detail(detail);
  }
  if (input.fund.has_value()) {
    FundDetail detail = *input.fund;
    detail.asset_id = asset.id;
    assets_.upsert_fund_detail(detail);
  }
  if (input.bond.has_value()) {
    BondDetail detail = *input.bond;
    detail.asset_id = asset.id;
    assets_.upsert_bond_detail(detail);
  }
  if (input.insurance.has_value()) {
    InsuranceDetail detail = *input.insurance;
    detail.asset_id = asset.id;
    assets_.upsert_insurance_detail(detail);
  }
  transaction.commit();

  return load_bundle(asset);
}

AssetBundle AssetService::get_bundle(std::int64_t id) { return load_bundle(get(id)); }

Asset AssetService::get(std::int64_t id) {
  const auto asset = assets_.find_by_id(id);
  if (!asset.has_value()) {
    throw not_found("asset not found");
  }
  return *asset;
}

std::vector<AssetBundle> AssetService::list_bundles(
    std::int64_t household_id, std::optional<std::int64_t> owner_member_id,
    std::optional<std::int64_t> account_id) {
  const auto assets = assets_.list_by_household(household_id, owner_member_id, account_id);
  std::vector<AssetBundle> bundles;
  bundles.reserve(assets.size());
  for (const auto& asset : assets) {
    bundles.push_back(load_bundle(asset));
  }
  return bundles;
}

AssetBundle AssetService::load_bundle(const Asset& asset) {
  AssetBundle bundle;
  bundle.asset = asset;
  switch (asset.asset_type) {
    case AssetType::TermDeposit:
      bundle.term_deposit = assets_.find_term_deposit_detail(asset.id);
      break;
    case AssetType::Fund:
      bundle.fund = assets_.find_fund_detail(asset.id);
      break;
    case AssetType::Bond:
      bundle.bond = assets_.find_bond_detail(asset.id);
      break;
    case AssetType::Insurance:
      bundle.insurance = assets_.find_insurance_detail(asset.id);
      break;
    default:
      break;
  }
  return bundle;
}

Asset AssetService::update_metadata(std::int64_t id, const std::string& name,
                                    std::optional<std::int64_t> opening_balance,
                                    const std::optional<std::string>& remark) {
  Asset asset = get(id);
  asset.name = strings::require_text(name, "name", 100);
  asset.remark = clean_optional(remark, "remark", 500);

  bool balance_changed = false;
  if (opening_balance.has_value() && *opening_balance != asset.opening_balance) {
    if (transactions_.count_by_asset(id) > 0) {
      throw conflict(
          "opening_balance cannot be changed after the asset has transactions");
    }
    validate_opening_balance(asset.asset_type, *opening_balance);
    const std::int64_t delta = *opening_balance - asset.opening_balance;
    asset.opening_balance = *opening_balance;
    asset.current_balance += delta;
    balance_changed = true;
  }

  asset.updated_at = time_util::now_iso8601();

  TransactionGuard transaction(database_);
  assets_.update_metadata(asset);
  if (balance_changed) {
    assets_.update_balance(asset.id, asset.current_balance, asset.updated_at);
  }
  transaction.commit();
  return asset;
}

Asset AssetService::update_status(std::int64_t id, AssetStatus status) {
  Asset asset = get(id);
  asset.status = status;
  asset.updated_at = time_util::now_iso8601();
  assets_.update_status(asset.id, status, asset.updated_at);
  return asset;
}

AssetBundle AssetService::update_detail(std::int64_t id, AssetType detail_type,
                                        const TermDepositDetail* term_deposit,
                                        const FundDetail* fund, const BondDetail* bond,
                                        const InsuranceDetail* insurance) {
  Asset asset = get(id);
  if (detail_type != asset.asset_type) {
    throw invalid_request("detail type must match asset_type");
  }
  switch (detail_type) {
    case AssetType::TermDeposit: {
      if (term_deposit == nullptr) {
        throw invalid_request("term_deposit detail is required");
      }
      TermDepositDetail detail = *term_deposit;
      detail.asset_id = id;
      assets_.upsert_term_deposit_detail(detail);
      break;
    }
    case AssetType::Fund: {
      if (fund == nullptr) {
        throw invalid_request("fund detail is required");
      }
      FundDetail detail = *fund;
      detail.asset_id = id;
      assets_.upsert_fund_detail(detail);
      break;
    }
    case AssetType::Bond: {
      if (bond == nullptr) {
        throw invalid_request("bond detail is required");
      }
      BondDetail detail = *bond;
      detail.asset_id = id;
      assets_.upsert_bond_detail(detail);
      break;
    }
    case AssetType::Insurance: {
      if (insurance == nullptr) {
        throw invalid_request("insurance detail is required");
      }
      InsuranceDetail detail = *insurance;
      detail.asset_id = id;
      assets_.upsert_insurance_detail(detail);
      break;
    }
    default:
      throw invalid_request("this asset type has no detail block");
  }
  return load_bundle(asset);
}

}  // namespace wt

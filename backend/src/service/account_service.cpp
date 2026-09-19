#include "service/account_service.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
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

}  // namespace

void AccountService::require_household(std::int64_t household_id) {
  if (!households_.find_by_id(household_id).has_value()) {
    throw not_found("household not found");
  }
}

void AccountService::require_member(std::int64_t household_id,
                                    std::int64_t member_id) {
  const auto member = members_.find_by_id(member_id);
  if (!member.has_value()) {
    throw not_found("member not found");
  }
  if (member->household_id != household_id) {
    throw invalid_request("member does not belong to the household");
  }
}

Account AccountService::create(std::int64_t household_id, std::int64_t owner_member_id,
                               const std::string& name, AccountType type,
                               const std::optional<std::string>& institution_name,
                               const std::optional<std::string>& account_no_masked,
                               const std::optional<std::string>& remark, bool enabled) {
  require_household(household_id);
  require_member(household_id, owner_member_id);

  Account account;
  account.household_id = household_id;
  account.owner_member_id = owner_member_id;
  account.name = strings::require_text(name, "name", 100);
  account.type = type;
  account.institution_name = clean_optional(institution_name, "institution_name", 100);
  account.account_no_masked =
      clean_optional(account_no_masked, "account_no_masked", 64);
  account.remark = clean_optional(remark, "remark", 500);
  account.enabled = enabled;
  account.created_at = time_util::now_iso8601();
  account.updated_at = account.created_at;
  account.id = accounts_.create(account);
  return account;
}

std::vector<Account> AccountService::list(std::int64_t household_id,
                                          std::optional<std::int64_t> owner_member_id) {
  require_household(household_id);
  return accounts_.list_by_household(household_id, owner_member_id);
}

std::vector<AccountView> AccountService::list_views(
    std::int64_t household_id, std::optional<std::int64_t> owner_member_id) {
  const auto accounts = list(household_id, owner_member_id);
  std::vector<AccountView> views;
  views.reserve(accounts.size());
  for (const auto& account : accounts) {
    AccountView view;
    view.account = account;
    view.balance = assets_.sum_balance_by_account(account.id);
    view.asset_count = assets_.count_by_account(account.id);
    views.push_back(std::move(view));
  }
  return views;
}

AccountView AccountService::get_view(std::int64_t id) {
  AccountView view;
  view.account = get(id);
  view.balance = assets_.sum_balance_by_account(id);
  view.asset_count = assets_.count_by_account(id);
  return view;
}

Account AccountService::get(std::int64_t id) {
  const auto account = accounts_.find_by_id(id);
  if (!account.has_value()) {
    throw not_found("account not found");
  }
  return *account;
}

Account AccountService::update(std::int64_t id, std::int64_t owner_member_id,
                               const std::string& name, AccountType type,
                               const std::optional<std::string>& institution_name,
                               const std::optional<std::string>& account_no_masked,
                               const std::optional<std::string>& remark, bool enabled) {
  Account account = get(id);
  if (owner_member_id != account.owner_member_id) {
    // Changing the owner would desynchronise the redundant owner_member_id that
    // assets and transactions keep. V1 blocks it once assets exist.
    if (assets_.count_by_account(id) > 0) {
      throw conflict("cannot change account owner while it still has assets");
    }
    require_member(account.household_id, owner_member_id);
    account.owner_member_id = owner_member_id;
  }
  account.name = strings::require_text(name, "name", 100);
  account.type = type;
  account.institution_name = clean_optional(institution_name, "institution_name", 100);
  account.account_no_masked =
      clean_optional(account_no_masked, "account_no_masked", 64);
  account.remark = clean_optional(remark, "remark", 500);
  account.enabled = enabled;
  account.updated_at = time_util::now_iso8601();
  accounts_.update(account);
  return account;
}

}  // namespace wt

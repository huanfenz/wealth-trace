#include "service/transaction_service.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

std::string resolved_time(const std::string& transaction_time) {
  if (strings::is_blank(transaction_time)) {
    return time_util::now_iso8601();
  }
  return time_util::require_datetime(transaction_time, "transaction_time");
}

std::optional<std::string> clean_category(const std::optional<std::string>& category) {
  if (!category.has_value()) {
    return std::nullopt;
  }
  const auto cleaned = strings::optional_text(*category, "category", 64);
  if (cleaned.empty()) {
    return std::nullopt;
  }
  return cleaned;
}

std::optional<std::string> clean_remark(const std::optional<std::string>& remark) {
  if (!remark.has_value()) {
    return std::nullopt;
  }
  const auto cleaned = strings::optional_text(*remark, "remark", 500);
  if (cleaned.empty()) {
    return std::nullopt;
  }
  return cleaned;
}

}  // namespace

Transaction TransactionService::record(
    TransactionType type, std::int64_t asset_id,
    const std::optional<std::string>& category, std::int64_t amount,
    const std::string& transaction_time, const std::optional<std::string>& remark) {
  if (amount == 0) {
    throw invalid_request("amount must not be zero");
  }
  if (type != TransactionType::Adjustment && amount < 0) {
    throw invalid_request("amount must be positive");
  }

  const auto asset = assets_.find_by_id(asset_id);
  if (!asset.has_value()) {
    throw not_found("asset not found");
  }
  if (asset->status != AssetStatus::Active) {
    throw conflict("asset is closed and cannot receive transactions");
  }

  const std::string now = time_util::now_iso8601();
  const std::string time = resolved_time(transaction_time);
  const std::int64_t delta = transaction_delta(type, amount);
  const std::int64_t new_balance = asset->current_balance + delta;

  Transaction transaction;
  transaction.household_id = asset->household_id;
  transaction.owner_member_id = asset->owner_member_id;
  transaction.asset_id = asset->id;
  transaction.type = type;
  transaction.category = clean_category(category);
  transaction.amount = amount;
  transaction.balance_before = asset->current_balance;
  transaction.balance_after = new_balance;
  transaction.transaction_time = time;
  transaction.remark = clean_remark(remark);
  transaction.status = TransactionStatus::Normal;
  transaction.created_at = now;
  transaction.updated_at = now;

  TransactionGuard guard(database_);
  transaction.id = transactions_.create(transaction);
  assets_.update_balance(asset->id, new_balance, now);
  guard.commit();
  return transaction;
}

Transaction TransactionService::record_income(
    std::int64_t asset_id, const std::optional<std::string>& category,
    std::int64_t amount, const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  return record(TransactionType::Income, asset_id, category, amount, transaction_time,
                remark);
}

Transaction TransactionService::record_expense(
    std::int64_t asset_id, const std::optional<std::string>& category,
    std::int64_t amount, const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  return record(TransactionType::Expense, asset_id, category, amount, transaction_time,
                remark);
}

Transaction TransactionService::record_adjustment(
    std::int64_t asset_id, std::int64_t amount, const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  return record(TransactionType::Adjustment, asset_id, std::nullopt, amount,
                transaction_time, remark);
}

TransferResult TransactionService::transfer(std::int64_t from_asset_id,
                                            std::int64_t to_asset_id,
                                            std::int64_t amount,
                                            const std::string& transaction_time,
                                            const std::optional<std::string>& remark) {
  if (amount <= 0) {
    throw invalid_request("transfer amount must be positive");
  }
  if (from_asset_id == to_asset_id) {
    throw invalid_request("cannot transfer to the same asset");
  }
  const auto from = assets_.find_by_id(from_asset_id);
  const auto to = assets_.find_by_id(to_asset_id);
  if (!from.has_value()) {
    throw not_found("source asset not found");
  }
  if (!to.has_value()) {
    throw not_found("target asset not found");
  }
  if (from->household_id != to->household_id) {
    throw invalid_request("cross-household transfer is not supported");
  }
  if (from->status != AssetStatus::Active || to->status != AssetStatus::Active) {
    throw conflict("transfer requires both assets to be active");
  }

  const std::string now = time_util::now_iso8601();
  const std::string time = resolved_time(transaction_time);
  const std::optional<std::string> cleaned_remark = clean_remark(remark);

  const std::int64_t new_from_balance = from->current_balance - amount;
  const std::int64_t new_to_balance = to->current_balance + amount;

  TransactionGuard guard(database_);
  const std::int64_t group_id = transactions_.next_transfer_group_id();

  Transaction outgoing;
  outgoing.household_id = from->household_id;
  outgoing.owner_member_id = from->owner_member_id;
  outgoing.asset_id = from->id;
  outgoing.type = TransactionType::TransferOut;
  outgoing.amount = amount;
  outgoing.transfer_group_id = group_id;
  outgoing.balance_before = from->current_balance;
  outgoing.balance_after = new_from_balance;
  outgoing.transaction_time = time;
  outgoing.remark = cleaned_remark;
  outgoing.status = TransactionStatus::Normal;
  outgoing.created_at = now;
  outgoing.updated_at = now;

  Transaction incoming;
  incoming.household_id = to->household_id;
  incoming.owner_member_id = to->owner_member_id;
  incoming.asset_id = to->id;
  incoming.type = TransactionType::TransferIn;
  incoming.amount = amount;
  incoming.transfer_group_id = group_id;
  incoming.balance_before = to->current_balance;
  incoming.balance_after = new_to_balance;
  incoming.transaction_time = time;
  incoming.remark = cleaned_remark;
  incoming.status = TransactionStatus::Normal;
  incoming.created_at = now;
  incoming.updated_at = now;

  outgoing.id = transactions_.create(outgoing);
  assets_.update_balance(from->id, new_from_balance, now);

  incoming.id = transactions_.create(incoming);
  assets_.update_balance(to->id, new_to_balance, now);

  guard.commit();

  TransferResult result;
  result.outgoing = outgoing;
  result.incoming = incoming;
  return result;
}

std::vector<Transaction> TransactionService::list(const TransactionQuery& query) {
  return transactions_.list(query);
}

std::int64_t TransactionService::count(const TransactionQuery& query) {
  return transactions_.count(query);
}

Transaction TransactionService::get(std::int64_t id) {
  const auto transaction = transactions_.find_by_id(id);
  if (!transaction.has_value()) {
    throw not_found("transaction not found");
  }
  return *transaction;
}

}  // namespace wt

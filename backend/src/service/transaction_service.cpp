// 交易服务实现：单资产流水与转账的记账，负责余额增量、事务边界与一致性校验。
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

// 交易时间缺省为当前 UTC 时间；显式传入则校验格式，非法抛 invalid_request。
std::string resolved_time(const std::string& transaction_time) {
  if (strings::is_blank(transaction_time)) {
    return time_util::now_iso8601();
  }
  return time_util::require_datetime(transaction_time, "transaction_time");
}

// 分类/备注清洗：未提供或去空白后为空都归一化为 nullopt（避免空串入库）。
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
    std::int64_t household_id, TransactionType type, std::int64_t asset_id,
    const std::optional<std::string>& category, std::int64_t amount,
    const std::string& transaction_time, const std::optional<std::string>& remark) {
  // 金额规则：任何类型都不得为 0；除 ADJUSTMENT 外，amount 统一存正数
  // 绝对值，方向完全由 type 决定（详见 transaction_delta）。
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
  if (asset->household_id != household_id) {
    throw invalid_request("asset does not belong to the household");
  }
  // 已关闭资产冻结：既不计入统计，也不能再产生流水。
  if (asset->status != AssetStatus::Active) {
    throw conflict("asset is closed and cannot receive transactions");
  }

  const std::string now = time_util::now_iso8601();
  const std::string time = resolved_time(transaction_time);
  // delta 为流水对余额的带符号影响；负债的负数余额在此自然继续变负。
  const std::int64_t delta = transaction_delta(type, amount);
  const std::int64_t new_balance = asset->current_balance + delta;

  Transaction transaction;
  // 冗余属主同样取自资产（而资产又取自账户），保证流水与资产归属一致。
  transaction.household_id = asset->household_id;
  transaction.owner_member_id = asset->owner_member_id;
  transaction.asset_id = asset->id;
  transaction.type = type;
  transaction.category = clean_category(category);
  transaction.amount = amount;
  // 快照前后余额，便于对账与追溯；账实不符时可据此定位。
  transaction.balance_before = asset->current_balance;
  transaction.balance_after = new_balance;
  transaction.transaction_time = time;
  transaction.remark = clean_remark(remark);
  transaction.status = TransactionStatus::Normal;
  transaction.created_at = now;
  transaction.updated_at = now;

  // 写流水与改余额必须原子：只在事务提交后 current_balance 才反映这笔交易，
  // 否则一旦失败会留下「余额变了却查不到流水」的不一致。
  TransactionGuard guard(database_);
  transaction.id = transactions_.create(transaction);
  assets_.update_balance(asset->id, new_balance, now);
  guard.commit();
  return transaction;
}

// 收入：余额 +amount。
Transaction TransactionService::record_income(
    std::int64_t household_id, std::int64_t asset_id,
    const std::optional<std::string>& category,
    std::int64_t amount, const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  std::scoped_lock lock(database_.mutex());
  return record(household_id, TransactionType::Income, asset_id, category, amount, transaction_time,
                remark);
}

// 支出：余额 -amount。
Transaction TransactionService::record_expense(
    std::int64_t household_id, std::int64_t asset_id,
    const std::optional<std::string>& category,
    std::int64_t amount, const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  std::scoped_lock lock(database_.mutex());
  return record(household_id, TransactionType::Expense, asset_id, category, amount, transaction_time,
                remark);
}

// 调整：余额直接 +amount（amount 可负）；调整无需分类，故 category 传 nullopt。
Transaction TransactionService::record_adjustment(
    std::int64_t household_id, std::int64_t asset_id, std::int64_t amount,
    const std::string& transaction_time,
    const std::optional<std::string>& remark) {
  std::scoped_lock lock(database_.mutex());
  return record(household_id, TransactionType::Adjustment, asset_id, std::nullopt, amount,
                transaction_time, remark);
}

TransferResult TransactionService::transfer(std::int64_t household_id,
                                            std::int64_t from_asset_id,
                                            std::int64_t to_asset_id,
                                            std::int64_t amount,
                                            const std::string& transaction_time,
                                            const std::optional<std::string>& remark) {
  std::scoped_lock lock(database_.mutex());
  // 转账金额必须为正：方向由「转出/转入」两个端点决定，不靠符号。
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
  // 仅支持同一家庭内部转账：跨家庭会破坏家庭资产边界的封闭性。
  if (from->household_id != household_id || to->household_id != household_id) {
    throw invalid_request("transfer assets do not belong to the household");
  }
  if (from->household_id != to->household_id) {
    throw invalid_request("cross-household transfer is not supported");
  }
  // 两端都必须在用，已关闭的资产不能转出也不能转入。
  if (from->status != AssetStatus::Active || to->status != AssetStatus::Active) {
    throw conflict("transfer requires both assets to be active");
  }

  const std::string now = time_util::now_iso8601();
  const std::string time = resolved_time(transaction_time);
  const std::optional<std::string> cleaned_remark = clean_remark(remark);

  // 转出减、转入加；负债作为转出方时负数余额会继续变小（更负）。
  const std::int64_t new_from_balance = from->current_balance - amount;
  const std::int64_t new_to_balance = to->current_balance + amount;

  // 关键：整个转账的四步（写转出流水 + 改转出余额 + 写转入流水 +
  // 改转入余额）必须在同一事务里，且两条流水共用 transfer_group_id，
  // 从而保证要么两边都成立、要么全部回滚，不会出现「钱凭空消失/产生」。
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

  // 四条写操作在同一事务内依次执行；任何一步失败都会由 guard 析构时回滚。
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
  std::scoped_lock lock(database_.mutex());
  return transactions_.list(query);
}

std::int64_t TransactionService::count(const TransactionQuery& query) {
  std::scoped_lock lock(database_.mutex());
  return transactions_.count(query);
}

Transaction TransactionService::get(std::int64_t id) {
  std::scoped_lock lock(database_.mutex());
  const auto transaction = transactions_.find_by_id(id);
  if (!transaction.has_value()) {
    throw not_found("transaction not found");
  }
  return *transaction;
}

}  // namespace wt

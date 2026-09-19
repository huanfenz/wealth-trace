#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace wt {

// ---------------------------------------------------------------------------
// Core enumerations. These mirror the core enumerations from the design
// document and are persisted as their upper-case string names.
// ---------------------------------------------------------------------------

enum class MemberRole { Owner, Member };
enum class MemberStatus { Active, Inactive };
enum class AccountType { Bank, Alipay, Wechat, Cash, Securities, Insurance, Other };
enum class AssetType { Cash, TermDeposit, Fund, Bond, Insurance, Liability, Other };
enum class AssetStatus { Active, Closed };
enum class TransactionType { Income, Expense, TransferIn, TransferOut, Adjustment };
enum class TransactionStatus { Normal, Void };
enum class TermUnit { Day, Month, Year };

std::string_view to_string(MemberRole value);
std::string_view to_string(MemberStatus value);
std::string_view to_string(AccountType value);
std::string_view to_string(AssetType value);
std::string_view to_string(AssetStatus value);
std::string_view to_string(TransactionType value);
std::string_view to_string(TransactionStatus value);
std::string_view to_string(TermUnit value);

std::optional<MemberRole> parse_member_role(std::string_view text);
std::optional<MemberStatus> parse_member_status(std::string_view text);
std::optional<AccountType> parse_account_type(std::string_view text);
std::optional<AssetType> parse_asset_type(std::string_view text);
std::optional<AssetStatus> parse_asset_status(std::string_view text);
std::optional<TransactionType> parse_transaction_type(std::string_view text);
std::optional<TransactionStatus> parse_transaction_status(std::string_view text);
std::optional<TermUnit> parse_term_unit(std::string_view text);

// Signed effect of a transaction on the related asset's current_balance.
// Non-transfer types use the same numeric rules as the design document:
//   INCOME / TRANSFER_IN  -> +amount
//   EXPENSE / TRANSFER_OUT-> -amount
//   ADJUSTMENT            -> +amount (amount may itself be negative)
std::int64_t transaction_delta(TransactionType type, std::int64_t amount);

}  // namespace wt

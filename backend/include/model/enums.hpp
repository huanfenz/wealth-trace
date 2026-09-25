#pragma once

// 领域枚举：成员/账户/资产/交易等类型与状态，并负责与数据库字符串的互相转换。

#include <cstdint>
#include <optional>
#include <string_view>

namespace wt {

// ---------------------------------------------------------------------------
// 核心枚举。与设计文档中的枚举保持一致；持久化时使用其大写字符串名
// （映射见 enums.cpp），因此枚举的声明顺序/底层数值不参与持久化。
// ---------------------------------------------------------------------------

enum class MemberRole { Owner, Member };
enum class MemberStatus { Active, Inactive };
enum class AccountType { Bank, Alipay, Wechat, Cash, Securities, Insurance, Other };
enum class AssetType { Cash, TermDeposit, StockFund, BondFund, FlexibleTerm, CommercialPension, Insurance, Liability, Other };
enum class AssetStatus { Active, Closed };
enum class TransactionType { Income, Expense, TransferIn, TransferOut, Adjustment };
enum class TransactionStatus { Normal, Void };
enum class TermUnit { Day, Month, Year };
// 债券基金持有方式：MinHolding=持有期债基，Rolling=滚动持有债基。
enum class HoldingMode { MinHolding, Rolling };

// 枚举 -> 数据库存储的大写字符串（如 MemberRole::Owner -> "OWNER"）。
std::string_view to_string(MemberRole value);
std::string_view to_string(MemberStatus value);
std::string_view to_string(AccountType value);
std::string_view to_string(AssetType value);
std::string_view to_string(AssetStatus value);
std::string_view to_string(TransactionType value);
std::string_view to_string(TransactionStatus value);
std::string_view to_string(TermUnit value);
std::string_view to_string(HoldingMode value);

// 数据库字符串 -> 枚举；无法识别时返回 std::nullopt（而非抛异常）。
std::optional<MemberRole> parse_member_role(std::string_view text);
std::optional<MemberStatus> parse_member_status(std::string_view text);
std::optional<AccountType> parse_account_type(std::string_view text);
std::optional<AssetType> parse_asset_type(std::string_view text);
std::optional<AssetStatus> parse_asset_status(std::string_view text);
std::optional<TransactionType> parse_transaction_type(std::string_view text);
std::optional<TransactionStatus> parse_transaction_status(std::string_view text);
std::optional<TermUnit> parse_term_unit(std::string_view text);
std::optional<HoldingMode> parse_holding_mode(std::string_view text);

// 一次交易对关联资产 current_balance 的带符号影响量（金额单位为「分」）。
// 非转账类型遵循设计文档规则：
//   INCOME / TRANSFER_IN   -> +amount（增加余额）
//   EXPENSE / TRANSFER_OUT -> -amount（减少余额）
//   ADJUSTMENT             -> +amount（amount 本身可为负，用于手工调账）
std::int64_t transaction_delta(TransactionType type, std::int64_t amount);

}  // namespace wt

#include "model/enums.hpp"

// 枚举转换实现：维护枚举与数据库大写字符串之间的双向映射。

#include <iterator>
#include <utility>

namespace wt {
namespace {

// 在一张 (字符串, 枚举) 表中线性查找匹配项；未命中返回 nullopt。
template <typename Enum>
std::optional<Enum> parse_enum(std::string_view text,
                               const std::pair<std::string_view, Enum>* table,
                               std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    if (table[i].first == text) {
      return table[i].second;
    }
  }
  return std::nullopt;
}

// 各枚举的持久化字符串表。使用 constexpr 编译期常量，避免运行时构造开销。
constexpr std::pair<std::string_view, MemberRole> kMemberRoles[] = {
    {"OWNER", MemberRole::Owner}, {"MEMBER", MemberRole::Member}};
constexpr std::pair<std::string_view, MemberStatus> kMemberStatuses[] = {
    {"ACTIVE", MemberStatus::Active}, {"INACTIVE", MemberStatus::Inactive}};
constexpr std::pair<std::string_view, AccountType> kAccountTypes[] = {
    {"BANK", AccountType::Bank},           {"ALIPAY", AccountType::Alipay},
    {"WECHAT", AccountType::Wechat},       {"CASH", AccountType::Cash},
    {"SECURITIES", AccountType::Securities}, {"INSURANCE", AccountType::Insurance},
    {"OTHER", AccountType::Other}};
constexpr std::pair<std::string_view, AssetType> kAssetTypes[] = {
    {"CASH", AssetType::Cash},         {"TERM_DEPOSIT", AssetType::TermDeposit},
    {"FUND", AssetType::Fund},         {"BOND", AssetType::Bond},
    {"INSURANCE", AssetType::Insurance}, {"LIABILITY", AssetType::Liability},
    {"OTHER", AssetType::Other}};
constexpr std::pair<std::string_view, AssetStatus> kAssetStatuses[] = {
    {"ACTIVE", AssetStatus::Active}, {"CLOSED", AssetStatus::Closed}};
constexpr std::pair<std::string_view, TransactionType> kTransactionTypes[] = {
    {"INCOME", TransactionType::Income},
    {"EXPENSE", TransactionType::Expense},
    {"TRANSFER_IN", TransactionType::TransferIn},
    {"TRANSFER_OUT", TransactionType::TransferOut},
    {"ADJUSTMENT", TransactionType::Adjustment}};
constexpr std::pair<std::string_view, TransactionStatus> kTransactionStatuses[] = {
    {"NORMAL", TransactionStatus::Normal}, {"VOID", TransactionStatus::Void}};
constexpr std::pair<std::string_view, TermUnit> kTermUnits[] = {
    {"DAY", TermUnit::Day}, {"MONTH", TermUnit::Month}, {"YEAR", TermUnit::Year}};

}  // namespace

// 以下 to_string：把枚举写成数据库存储用的大写字符串。
// switch 之后的返回值是兜底，正常情况下不可达（所有枚举分支均已覆盖）。
std::string_view to_string(MemberRole value) {
  return value == MemberRole::Owner ? "OWNER" : "MEMBER";
}
std::string_view to_string(MemberStatus value) {
  return value == MemberStatus::Active ? "ACTIVE" : "INACTIVE";
}
std::string_view to_string(AccountType value) {
  switch (value) {
    case AccountType::Bank: return "BANK";
    case AccountType::Alipay: return "ALIPAY";
    case AccountType::Wechat: return "WECHAT";
    case AccountType::Cash: return "CASH";
    case AccountType::Securities: return "SECURITIES";
    case AccountType::Insurance: return "INSURANCE";
    case AccountType::Other: return "OTHER";
  }
  return "OTHER";
}
std::string_view to_string(AssetType value) {
  switch (value) {
    case AssetType::Cash: return "CASH";
    case AssetType::TermDeposit: return "TERM_DEPOSIT";
    case AssetType::Fund: return "FUND";
    case AssetType::Bond: return "BOND";
    case AssetType::Insurance: return "INSURANCE";
    case AssetType::Liability: return "LIABILITY";
    case AssetType::Other: return "OTHER";
  }
  return "OTHER";
}
std::string_view to_string(AssetStatus value) {
  return value == AssetStatus::Active ? "ACTIVE" : "CLOSED";
}
std::string_view to_string(TransactionType value) {
  switch (value) {
    case TransactionType::Income: return "INCOME";
    case TransactionType::Expense: return "EXPENSE";
    case TransactionType::TransferIn: return "TRANSFER_IN";
    case TransactionType::TransferOut: return "TRANSFER_OUT";
    case TransactionType::Adjustment: return "ADJUSTMENT";
  }
  return "ADJUSTMENT";
}
std::string_view to_string(TransactionStatus value) {
  return value == TransactionStatus::Normal ? "NORMAL" : "VOID";
}
std::string_view to_string(TermUnit value) {
  switch (value) {
    case TermUnit::Day: return "DAY";
    case TermUnit::Month: return "MONTH";
    case TermUnit::Year: return "YEAR";
  }
  return "YEAR";
}

// 以下 parse_*：把数据库字符串解析回枚举，未知取值统一返回 nullopt。
std::optional<MemberRole> parse_member_role(std::string_view text) {
  return parse_enum(text, kMemberRoles, std::size(kMemberRoles));
}
std::optional<MemberStatus> parse_member_status(std::string_view text) {
  return parse_enum(text, kMemberStatuses, std::size(kMemberStatuses));
}
std::optional<AccountType> parse_account_type(std::string_view text) {
  return parse_enum(text, kAccountTypes, std::size(kAccountTypes));
}
std::optional<AssetType> parse_asset_type(std::string_view text) {
  return parse_enum(text, kAssetTypes, std::size(kAssetTypes));
}
std::optional<AssetStatus> parse_asset_status(std::string_view text) {
  return parse_enum(text, kAssetStatuses, std::size(kAssetStatuses));
}
std::optional<TransactionType> parse_transaction_type(std::string_view text) {
  return parse_enum(text, kTransactionTypes, std::size(kTransactionTypes));
}
std::optional<TransactionStatus> parse_transaction_status(std::string_view text) {
  return parse_enum(text, kTransactionStatuses, std::size(kTransactionStatuses));
}
std::optional<TermUnit> parse_term_unit(std::string_view text) {
  return parse_enum(text, kTermUnits, std::size(kTermUnits));
}

std::int64_t transaction_delta(TransactionType type, std::int64_t amount) {
  // 符号规则：收入/转入/调账为增量，支出/转出为减量；调账的 amount 自身可为负。
  switch (type) {
    case TransactionType::Income:
    case TransactionType::TransferIn:
    case TransactionType::Adjustment:
      return amount;
    case TransactionType::Expense:
    case TransactionType::TransferOut:
      return -amount;
  }
  return 0;
}

}  // namespace wt

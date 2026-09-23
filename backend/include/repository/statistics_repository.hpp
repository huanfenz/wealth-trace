// 统计查询仓储：对 asset / "transaction" 做聚合，返回净资产与收支口径数据。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/enums.hpp"

namespace wt {

class Database;

// 名称 + 金额（分）的通用统计行。
struct NamedAmount {
  std::int64_t id = 0;
  std::string name;
  std::int64_t amount = 0;
};

// 按资产类型汇总的一行。
struct TypeAmount {
  AssetType type = AssetType::Other;
  std::int64_t amount = 0;
};

// 按收支分类汇总的一行。
struct CategoryAmount {
  std::string category;
  std::int64_t amount = 0;
};

// 收入/支出汇总，金额单位均为分。
struct IncomeExpenseSummary {
  std::int64_t income = 0;
  std::int64_t expense = 0;
  std::int64_t balance() const { return income - expense; }
};

// 单月收支汇总：month 为 "YYYY-MM"，金额单位为分。
struct MonthlyIncomeExpense {
  std::string month;
  std::int64_t income = 0;
  std::int64_t expense = 0;
  std::int64_t balance() const { return income - expense; }
};

// 统计仓储。金额口径：整数分；资产聚合均只统计 status='ACTIVE'。
class StatisticsRepository {
 public:
  explicit StatisticsRepository(Database& database) : database_(database) {}

  // Net worth components (minor units). Liabilities are stored as negative
  // current_balance values, so `total_balance` is the household net worth.
  // 总资产：排除 LIABILITY 的 ACTIVE 资产 current_balance 求和。
  std::int64_t total_assets(std::int64_t household_id);
  // 总负债：LIABILITY 的 ACTIVE 资产 current_balance 求和后取 ABS（正数）。
  std::int64_t total_liabilities(std::int64_t household_id);
  // 净资产：全部 ACTIVE 资产 current_balance 求和（负债本身为负数，直接相加）。
  std::int64_t total_balance(std::int64_t household_id);
  // 某成员名下 ACTIVE 资产的 current_balance 之和。
  std::int64_t member_balance(std::int64_t household_id, std::int64_t member_id);

  // 按成员汇总 ACTIVE 资产，联 household_member 取名字，金额降序。
  std::vector<NamedAmount> assets_by_member(std::int64_t household_id);
  // 按账户汇总 ACTIVE 资产，联 account 取名字，金额降序。
  std::vector<NamedAmount> assets_by_account(std::int64_t household_id);
  // 按资产类型汇总 ACTIVE 资产，金额降序。
  std::vector<TypeAmount> assets_by_type(std::int64_t household_id);

  // 区间收支汇总：只统计 status='NORMAL' 且 type IN ('INCOME','EXPENSE')；
  // member_id 有值时限定该成员。时间闭区间 [from_time, to_time]。
  IncomeExpenseSummary income_expense(std::int64_t household_id,
                                      std::optional<std::int64_t> member_id,
                                      const std::string& from_time,
                                      const std::string& to_time);
  // 按成员汇总净收支（INCOME 记正、EXPENSE 记负），LEFT JOIN 保证无交易成员也返回 0。
  std::vector<NamedAmount> income_expense_by_member(std::int64_t household_id,
                                                    const std::string& from_time,
                                                    const std::string& to_time);
  // 按分类汇总区间支出（type='EXPENSE'，NORMAL），金额降序。
  std::vector<CategoryAmount> expense_by_category(std::int64_t household_id,
                                                  const std::string& from_time,
                                                  const std::string& to_time);
  // 按分类汇总区间收入（type='INCOME'，NORMAL），金额降序。
  std::vector<CategoryAmount> income_by_category(std::int64_t household_id,
                                                 const std::string& from_time,
                                                 const std::string& to_time);

 private:
  Database& database_;
};

}  // namespace wt

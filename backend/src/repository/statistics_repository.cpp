// statistics_repository.cpp：资产与收支的聚合统计 SQL；只做查询，不写业务规则。
#include "repository/statistics_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {

// 总资产口径：排除 type='LIABILITY' 的 ACTIVE 资产，对 current_balance（分）求和。
std::int64_t StatisticsRepository::total_assets(std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT COALESCE(SUM(current_balance), 0) FROM asset "
      "WHERE household_id = ? AND asset_type <> 'LIABILITY' AND status = 'ACTIVE';");
  statement.bind(1, household_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 总负债口径：type='LIABILITY' 的 ACTIVE 资产，current_balance 为负数，
// 故对外用 ABS(SUM(...)) 返回正数金额。
std::int64_t StatisticsRepository::total_liabilities(std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT COALESCE(ABS(SUM(current_balance)), 0) FROM asset "
      "WHERE household_id = ? AND asset_type = 'LIABILITY' AND status = 'ACTIVE';");
  statement.bind(1, household_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 净资产口径：全部 ACTIVE 资产 current_balance 直接求和（负债已是负数，无需再减）。
std::int64_t StatisticsRepository::total_balance(std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT COALESCE(SUM(current_balance), 0) FROM asset "
      "WHERE household_id = ? AND status = 'ACTIVE';");
  statement.bind(1, household_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 成员资产口径：该成员名下 ACTIVE 资产 current_balance 求和（含负债负数）。
std::int64_t StatisticsRepository::member_balance(std::int64_t household_id,
                                                  std::int64_t member_id) {
  Statement statement(
      database_,
      "SELECT COALESCE(SUM(current_balance), 0) FROM asset "
      "WHERE household_id = ? AND owner_member_id = ? AND status = 'ACTIVE';");
  statement.bind(1, household_id).bind(2, member_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 按成员分组：仅 ACTIVE 资产；JOIN 成员表取姓名；ORDER BY 3 即按汇总金额降序。
std::vector<NamedAmount> StatisticsRepository::assets_by_member(
    std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT a.owner_member_id, m.name, COALESCE(SUM(a.current_balance), 0) "
      "FROM asset a JOIN household_member m ON m.id = a.owner_member_id "
      "WHERE a.household_id = ? AND a.status = 'ACTIVE' "
      "GROUP BY a.owner_member_id, m.name ORDER BY 3 DESC;");
  statement.bind(1, household_id);
  std::vector<NamedAmount> result;
  while (statement.step()) {
    result.push_back({statement.get_int64(0), statement.get_text(1),
                      statement.get_int64(2)});
  }
  return result;
}

// 按账户分组：仅 ACTIVE 资产；JOIN 账户表取名称；按汇总金额降序。
std::vector<NamedAmount> StatisticsRepository::assets_by_account(
    std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT ac.id, ac.name, COALESCE(SUM(a.current_balance), 0) "
      "FROM asset a JOIN account ac ON ac.id = a.account_id "
      "WHERE a.household_id = ? AND a.status = 'ACTIVE' "
      "GROUP BY ac.id, ac.name ORDER BY 3 DESC;");
  statement.bind(1, household_id);
  std::vector<NamedAmount> result;
  while (statement.step()) {
    result.push_back({statement.get_int64(0), statement.get_text(1),
                      statement.get_int64(2)});
  }
  return result;
}

// 按资产类型分组：仅 ACTIVE 资产，含 LIABILITY 类型；按汇总金额降序。
std::vector<TypeAmount> StatisticsRepository::assets_by_type(std::int64_t household_id) {
  Statement statement(
      database_,
      "SELECT asset_type, COALESCE(SUM(current_balance), 0) FROM asset "
      "WHERE household_id = ? AND status = 'ACTIVE' GROUP BY asset_type ORDER BY 2 DESC;");
  statement.bind(1, household_id);
  std::vector<TypeAmount> result;
  while (statement.step()) {
    result.push_back(
        {parse_asset_type(statement.get_text(0)).value_or(AssetType::Other),
         statement.get_int64(1)});
  }
  return result;
}

// 收支汇总口径：status='NORMAL'，type 仅取 INCOME/EXPENSE，时间闭区间；
// member_id 可选，有值时在时间参数之后追加绑定第 4 个参数。
// 结果按 type 分组，逐行回填到 summary（收入/支出各一行）。
IncomeExpenseSummary StatisticsRepository::income_expense(
    std::int64_t household_id, std::optional<std::int64_t> member_id,
    const std::string& from_time, const std::string& to_time) {
  std::string sql =
      "SELECT type, COALESCE(SUM(amount), 0) FROM \"transaction\" "
      "WHERE household_id = ? AND status = 'NORMAL' "
      "AND type IN ('INCOME', 'EXPENSE') AND transaction_time >= ? AND transaction_time <= ?";
  if (member_id.has_value()) {
    sql += " AND owner_member_id = ?";
  }
  sql += " GROUP BY type;";

  Statement statement(database_, sql);
  statement.bind(1, household_id).bind(2, from_time).bind(3, to_time);
  if (member_id.has_value()) {
    statement.bind(4, *member_id);
  }
  IncomeExpenseSummary summary;
  while (statement.step()) {
    const auto type = parse_transaction_type(statement.get_text(0));
    const std::int64_t amount = statement.get_int64(1);
    if (type == TransactionType::Income) {
      summary.income = amount;
    } else if (type == TransactionType::Expense) {
      summary.expense = amount;
    }
  }
  return summary;
}

// 成员净收支口径：status='NORMAL'，INCOME 记 +amount、EXPENSE 记 -amount，
// 其他类型记 0；LEFT JOIN 使无交易的成员也保留并返回 0。
std::vector<NamedAmount> StatisticsRepository::income_expense_by_member(
    std::int64_t household_id, const std::string& from_time, const std::string& to_time) {
  Statement statement(
      database_,
      "SELECT m.id, m.name, "
      "COALESCE(SUM(CASE WHEN t.type = 'INCOME' THEN t.amount "
      "                  WHEN t.type = 'EXPENSE' THEN -t.amount ELSE 0 END), 0) "
      "FROM household_member m "
      "LEFT JOIN \"transaction\" t ON t.owner_member_id = m.id AND t.status = 'NORMAL' "
      "  AND t.transaction_time >= ? AND t.transaction_time <= ? "
      "WHERE m.household_id = ? GROUP BY m.id, m.name ORDER BY m.id ASC;");
  statement.bind(1, from_time).bind(2, to_time).bind(3, household_id);
  std::vector<NamedAmount> result;
  while (statement.step()) {
    result.push_back({statement.get_int64(0), statement.get_text(1),
                      statement.get_int64(2)});
  }
  return result;
}

// 支出分类口径：status='NORMAL' 且 type='EXPENSE'，按 category 分组，
// category 为空时归并为 '未分类'，按金额降序。
std::vector<CategoryAmount> StatisticsRepository::expense_by_category(
    std::int64_t household_id, const std::string& from_time, const std::string& to_time) {
  Statement statement(
      database_,
      "SELECT COALESCE(c.name, '未分类'), COALESCE(SUM(t.amount), 0) FROM \"transaction\" t "
      "LEFT JOIN transaction_category c ON c.id = t.category_id "
      "WHERE t.household_id = ? AND t.status = 'NORMAL' AND t.type = 'EXPENSE' "
      "AND t.transaction_time >= ? AND t.transaction_time <= ? "
      "GROUP BY t.category_id, c.name ORDER BY 2 DESC;");
  statement.bind(1, household_id).bind(2, from_time).bind(3, to_time);
  std::vector<CategoryAmount> result;
  while (statement.step()) {
    result.push_back({statement.get_text(0), statement.get_int64(1)});
  }
  return result;
}

// 收入分类口径：status='NORMAL' 且 type='INCOME'，按 category 分组，
// category 为空时归并为 '未分类'，按金额降序。
std::vector<CategoryAmount> StatisticsRepository::income_by_category(
    std::int64_t household_id, const std::string& from_time, const std::string& to_time) {
  Statement statement(
      database_,
      "SELECT COALESCE(c.name, '未分类'), COALESCE(SUM(t.amount), 0) FROM \"transaction\" t "
      "LEFT JOIN transaction_category c ON c.id = t.category_id "
      "WHERE t.household_id = ? AND t.status = 'NORMAL' AND t.type = 'INCOME' "
      "AND t.transaction_time >= ? AND t.transaction_time <= ? "
      "GROUP BY t.category_id, c.name ORDER BY 2 DESC;");
  statement.bind(1, household_id).bind(2, from_time).bind(3, to_time);
  std::vector<CategoryAmount> result;
  while (statement.step()) {
    result.push_back({statement.get_text(0), statement.get_int64(1)});
  }
  return result;
}

}  // namespace wt

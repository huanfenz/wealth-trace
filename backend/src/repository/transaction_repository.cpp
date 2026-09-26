// transaction_repository.cpp："transaction" 表的 SQL 实现与行映射；
// 动态过滤条件由 build_where 生成、apply_bindings 统一绑定。
#include "repository/transaction_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

// 行映射：列下标必须与 kSelectColumns 的顺序严格一致。
// 0=id 1=household_id 2=owner_member_id 3=asset_id 4=type 5=category_id 6=category 7=amount
// 8=transfer_group_id 9=balance_before 10=balance_after 11=transaction_time
// 12=remark 13=status 14=created_at 15=updated_at
// 金额单位：分。枚举解析失败回退 Adjustment / Normal。
Transaction map_transaction(Statement& statement) {
  Transaction transaction;
  transaction.id = statement.get_int64(0);
  transaction.household_id = statement.get_int64(1);
  transaction.owner_member_id = statement.get_int64(2);
  transaction.asset_id = statement.get_int64(3);
  transaction.type =
      parse_transaction_type(statement.get_text(4)).value_or(TransactionType::Adjustment);
  transaction.category_id = statement.get_optional_int64(5);
  transaction.category = statement.get_optional_text(6);
  transaction.amount = statement.get_int64(7);
  transaction.transfer_group_id = statement.get_optional_int64(8);
  transaction.balance_before = statement.get_optional_int64(9);
  transaction.balance_after = statement.get_optional_int64(10);
  transaction.transaction_time = statement.get_text(11);
  transaction.remark = statement.get_optional_text(12);
  transaction.status =
      parse_transaction_status(statement.get_text(13)).value_or(TransactionStatus::Normal);
  transaction.created_at = statement.get_text(14);
  transaction.updated_at = statement.get_text(15);
  return transaction;
}

// SELECT 列顺序，与 map_transaction 的下标一一对应。
constexpr const char* kSelectColumns =
    "id, household_id, owner_member_id, asset_id, type, category_id, category, amount, "
    "transfer_group_id, balance_before, balance_after, transaction_time, remark, status, "
    "created_at, updated_at";

// Builds the WHERE clause shared by list() and count(). Bind parameters are
// appended to `bindings` in sqlite order.
// 动态 WHERE：household_id 必选；其余可选字段按固定顺序（成员、资产、类型、
// 起始时间、结束时间）逐个判断，有值才拼接 " AND 列 = ?"，并记录占位符序号 index。
// 文本参数与整数参数分开收集，序号与 SQL 中 ? 的位置一一对应。
// list()/count() 共用此函数，保证两处过滤口径完全一致。
std::string build_where(const TransactionQuery& query,
                        std::vector<std::pair<int, std::string>>& text_bindings,
                        std::vector<std::pair<int, std::int64_t>>& int_bindings) {
  std::string sql = " WHERE household_id = ?";
  int index = 1;
  int_bindings.emplace_back(index++, query.household_id);
  if (query.owner_member_id.has_value()) {
    sql += " AND owner_member_id = ?";
    int_bindings.emplace_back(index++, *query.owner_member_id);
  }
  if (query.asset_id.has_value()) {
    sql += " AND asset_id = ?";
    int_bindings.emplace_back(index++, *query.asset_id);
  }
  if (query.type.has_value()) {
    sql += " AND type = ?";
    text_bindings.emplace_back(index++, std::string(to_string(*query.type)));
  }
  if (query.from_time.has_value()) {
    sql += " AND transaction_time >= ?";
    text_bindings.emplace_back(index++, *query.from_time);
  }
  if (query.to_time.has_value()) {
    sql += " AND transaction_time <= ?";
    text_bindings.emplace_back(index++, *query.to_time);
  }
  return sql;
}

// 按 build_where 记录的序号绑定参数；文本与整数分两轮绑定，顺序不影响结果。
void apply_bindings(Statement& statement,
                    const std::vector<std::pair<int, std::string>>& text_bindings,
                    const std::vector<std::pair<int, std::int64_t>>& int_bindings) {
  for (const auto& [index, value] : text_bindings) {
    statement.bind(index, value);
  }
  for (const auto& [index, value] : int_bindings) {
    statement.bind(index, value);
  }
}

}  // namespace

// 插入流水；表名为 SQL 保留字故写作 "transaction"。枚举以文本持久化。
std::int64_t TransactionRepository::create(const Transaction& transaction) {
  Statement statement(
      database_,
      "INSERT INTO \"transaction\" (household_id, owner_member_id, asset_id, type, category_id, category, "
      "amount, transfer_group_id, balance_before, balance_after, transaction_time, "
      "remark, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  statement.bind(1, transaction.household_id)
      .bind(2, transaction.owner_member_id)
      .bind(3, transaction.asset_id)
      .bind(4, std::string(to_string(transaction.type)))
      .bind_optional_int64(5, transaction.category_id)
      .bind_optional_text(6, transaction.category)
      .bind(7, transaction.amount)
      .bind_optional_int64(8, transaction.transfer_group_id)
      .bind_optional_int64(9, transaction.balance_before)
      .bind_optional_int64(10, transaction.balance_after)
      .bind(11, transaction.transaction_time)
      .bind_optional_text(12, transaction.remark)
      .bind(13, std::string(to_string(transaction.status)))
      .bind(14, transaction.created_at)
      .bind(15, transaction.updated_at)
      .run();
  return database_.last_insert_rowid();
}

std::optional<Transaction> TransactionRepository::find_by_id(std::int64_t id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM \"transaction\" WHERE id = ?;");
  statement.bind(1, id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_transaction(statement);
}

bool TransactionRepository::update_category(
    std::int64_t id, std::optional<std::int64_t> category_id,
    const std::optional<std::string>& category, const std::string& updated_at) {
  Statement statement(database_,
                      "UPDATE \"transaction\" SET category_id = ?, category = ?, "
                      "updated_at = ? WHERE id = ?;");
  statement.bind_optional_int64(1, category_id)
      .bind_optional_text(2, category)
      .bind(3, updated_at)
      .bind(4, id)
      .run();
  return database_.changes() > 0;
}

// 按转账分组查询配对流水，按 id 升序（转出行先创建，通常排在前）。
std::vector<Transaction> TransactionRepository::list_by_transfer_group(
    std::int64_t group_id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM \"transaction\" WHERE transfer_group_id = ? "
                                     "ORDER BY id ASC;");
  statement.bind(1, group_id);
  std::vector<Transaction> transactions;
  while (statement.step()) {
    transactions.push_back(map_transaction(statement));
  }
  return transactions;
}

bool TransactionRepository::remove(std::int64_t id) {
  Statement statement(database_, "DELETE FROM \"transaction\" WHERE id = ?;");
  statement.bind(1, id);
  statement.run();
  return database_.changes() > 0;
}

// 列表：拼出 SELECT + build_where + 排序分页，先绑定 WHERE 参数再绑定 LIMIT/OFFSET。
std::vector<Transaction> TransactionRepository::list(const TransactionQuery& query) {
  std::vector<std::pair<int, std::string>> text_bindings;
  std::vector<std::pair<int, std::int64_t>> int_bindings;
  std::string sql = std::string("SELECT ") + kSelectColumns + " FROM \"transaction\"" +
                    build_where(query, text_bindings, int_bindings) +
                    " ORDER BY transaction_time DESC, id DESC LIMIT ? OFFSET ?;";

  // WHERE 里的占位符已占用 1..N，LIMIT/OFFSET 顺延为 N+1、N+2。
  const int limit_index = 1 + static_cast<int>(text_bindings.size() + int_bindings.size());
  Statement statement(database_, sql);
  apply_bindings(statement, text_bindings, int_bindings);
  statement.bind(limit_index, static_cast<std::int64_t>(query.limit));
  statement.bind(limit_index + 1, static_cast<std::int64_t>(query.offset));

  std::vector<Transaction> transactions;
  while (statement.step()) {
    transactions.push_back(map_transaction(statement));
  }
  return transactions;
}

// 计数：与 list 复用同一 build_where，确保总数与分页查询口径一致。
std::int64_t TransactionRepository::count(const TransactionQuery& query) {
  std::vector<std::pair<int, std::string>> text_bindings;
  std::vector<std::pair<int, std::int64_t>> int_bindings;
  std::string sql = std::string("SELECT COUNT(*) FROM \"transaction\"") +
                    build_where(query, text_bindings, int_bindings) + ";";

  Statement statement(database_, sql);
  apply_bindings(statement, text_bindings, int_bindings);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 统计某资产的历史流水条数。
std::int64_t TransactionRepository::count_by_asset(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT COUNT(*) FROM \"transaction\" WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

// 转账分组 id 生成：取现有最大值 + 1；表为空时 COALESCE 使结果为 1。
std::int64_t TransactionRepository::next_transfer_group_id() {
  Statement statement(database_,
                      "SELECT COALESCE(MAX(transfer_group_id), 0) + 1 FROM \"transaction\";");
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 1;
}

}  // namespace wt

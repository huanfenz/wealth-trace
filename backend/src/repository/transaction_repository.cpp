#include "repository/transaction_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

Transaction map_transaction(Statement& statement) {
  Transaction transaction;
  transaction.id = statement.get_int64(0);
  transaction.household_id = statement.get_int64(1);
  transaction.owner_member_id = statement.get_int64(2);
  transaction.asset_id = statement.get_int64(3);
  transaction.type =
      parse_transaction_type(statement.get_text(4)).value_or(TransactionType::Adjustment);
  transaction.category = statement.get_optional_text(5);
  transaction.amount = statement.get_int64(6);
  transaction.transfer_group_id = statement.get_optional_int64(7);
  transaction.balance_before = statement.get_optional_int64(8);
  transaction.balance_after = statement.get_optional_int64(9);
  transaction.transaction_time = statement.get_text(10);
  transaction.remark = statement.get_optional_text(11);
  transaction.status =
      parse_transaction_status(statement.get_text(12)).value_or(TransactionStatus::Normal);
  transaction.created_at = statement.get_text(13);
  transaction.updated_at = statement.get_text(14);
  return transaction;
}

constexpr const char* kSelectColumns =
    "id, household_id, owner_member_id, asset_id, type, category, amount, "
    "transfer_group_id, balance_before, balance_after, transaction_time, remark, status, "
    "created_at, updated_at";

// Builds the WHERE clause shared by list() and count(). Bind parameters are
// appended to `bindings` in sqlite order.
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

std::int64_t TransactionRepository::create(const Transaction& transaction) {
  Statement statement(
      database_,
      "INSERT INTO \"transaction\" (household_id, owner_member_id, asset_id, type, category, "
      "amount, transfer_group_id, balance_before, balance_after, transaction_time, "
      "remark, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  statement.bind(1, transaction.household_id)
      .bind(2, transaction.owner_member_id)
      .bind(3, transaction.asset_id)
      .bind(4, std::string(to_string(transaction.type)))
      .bind_optional_text(5, transaction.category)
      .bind(6, transaction.amount)
      .bind_optional_int64(7, transaction.transfer_group_id)
      .bind_optional_int64(8, transaction.balance_before)
      .bind_optional_int64(9, transaction.balance_after)
      .bind(10, transaction.transaction_time)
      .bind_optional_text(11, transaction.remark)
      .bind(12, std::string(to_string(transaction.status)))
      .bind(13, transaction.created_at)
      .bind(14, transaction.updated_at)
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

std::vector<Transaction> TransactionRepository::list(const TransactionQuery& query) {
  std::vector<std::pair<int, std::string>> text_bindings;
  std::vector<std::pair<int, std::int64_t>> int_bindings;
  std::string sql = std::string("SELECT ") + kSelectColumns + " FROM \"transaction\"" +
                    build_where(query, text_bindings, int_bindings) +
                    " ORDER BY transaction_time DESC, id DESC LIMIT ? OFFSET ?;";

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

std::int64_t TransactionRepository::count_by_asset(std::int64_t asset_id) {
  Statement statement(database_,
                      "SELECT COUNT(*) FROM \"transaction\" WHERE asset_id = ?;");
  statement.bind(1, asset_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

std::int64_t TransactionRepository::next_transfer_group_id() {
  Statement statement(database_,
                      "SELECT COALESCE(MAX(transfer_group_id), 0) + 1 FROM \"transaction\";");
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 1;
}

}  // namespace wt

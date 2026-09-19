// account_repository.cpp：account 表的 CRUD SQL 实现与行映射。
#include "repository/account_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

// 行映射：列下标必须与 kSelectColumns 的顺序严格一致。
// 0=id 1=household_id 2=owner_member_id 3=name 4=type
// 5=institution_name 6=account_no_masked 7=remark 8=enabled 9=created_at 10=updated_at
// 可空列用 get_optional_text；type 解析失败回退 AccountType::Other。
Account map_account(Statement& statement) {
  Account account;
  account.id = statement.get_int64(0);
  account.household_id = statement.get_int64(1);
  account.owner_member_id = statement.get_int64(2);
  account.name = statement.get_text(3);
  account.type = parse_account_type(statement.get_text(4)).value_or(AccountType::Other);
  account.institution_name = statement.get_optional_text(5);
  account.account_no_masked = statement.get_optional_text(6);
  account.remark = statement.get_optional_text(7);
  account.enabled = statement.get_bool(8);
  account.created_at = statement.get_text(9);
  account.updated_at = statement.get_text(10);
  return account;
}

// SELECT 列顺序，与 map_account 的下标一一对应。
constexpr const char* kSelectColumns =
    "id, household_id, owner_member_id, name, type, institution_name, "
    "account_no_masked, remark, enabled, created_at, updated_at";

}  // namespace

// 插入账户（enum 文本持久化，可空列走 bind_optional_text），返回自增主键。
std::int64_t AccountRepository::create(const Account& account) {
  Statement statement(
      database_,
      "INSERT INTO account (household_id, owner_member_id, name, type, institution_name, "
      "account_no_masked, remark, enabled, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
  statement.bind(1, account.household_id)
      .bind(2, account.owner_member_id)
      .bind(3, account.name)
      .bind(4, std::string(to_string(account.type)))
      .bind_optional_text(5, account.institution_name)
      .bind_optional_text(6, account.account_no_masked)
      .bind_optional_text(7, account.remark)
      .bind(8, account.enabled)
      .bind(9, account.created_at)
      .bind(10, account.updated_at)
      .run();
  return database_.last_insert_rowid();
}

std::optional<Account> AccountRepository::find_by_id(std::int64_t id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM account WHERE id = ?;");
  statement.bind(1, id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_account(statement);
}

// 动态查询：household_id 为必选条件；仅当 owner_member_id 有值时
// 才追加 " AND owner_member_id = ?" 并按顺序绑定第 2 个参数。
std::vector<Account> AccountRepository::list_by_household(
    std::int64_t household_id, std::optional<std::int64_t> owner_member_id) {
  std::string sql = std::string("SELECT ") + kSelectColumns +
                    " FROM account WHERE household_id = ?";
  if (owner_member_id.has_value()) {
    sql += " AND owner_member_id = ?";
  }
  sql += " ORDER BY id ASC;";

  Statement statement(database_, sql);
  statement.bind(1, household_id);
  if (owner_member_id.has_value()) {
    statement.bind(2, *owner_member_id);
  }
  std::vector<Account> accounts;
  while (statement.step()) {
    accounts.push_back(map_account(statement));
  }
  return accounts;
}

bool AccountRepository::update(const Account& account) {
  Statement statement(
      database_,
      "UPDATE account SET owner_member_id = ?, name = ?, type = ?, institution_name = ?, "
      "account_no_masked = ?, remark = ?, enabled = ?, updated_at = ? "
      "WHERE id = ?;");
  statement.bind(1, account.owner_member_id)
      .bind(2, account.name)
      .bind(3, std::string(to_string(account.type)))
      .bind_optional_text(4, account.institution_name)
      .bind_optional_text(5, account.account_no_masked)
      .bind_optional_text(6, account.remark)
      .bind(7, account.enabled)
      .bind(8, account.updated_at)
      .bind(9, account.id)
      .run();
  return database_.changes() > 0;
}

bool AccountRepository::exists(std::int64_t id) {
  Statement statement(database_, "SELECT COUNT(*) FROM account WHERE id = ?;");
  statement.bind(1, id);
  if (statement.step()) {
    return statement.get_int64(0) > 0;
  }
  return false;
}

}  // namespace wt

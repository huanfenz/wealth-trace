// household_repository.cpp：household 表的 CRUD SQL 实现与行映射。
#include "repository/household_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

// 行映射：列下标必须与 kSelectColumns 的顺序严格一致。
// 0=id 1=name 2=created_at 3=updated_at
Household map_household(Statement& statement) {
  Household household;
  household.id = statement.get_int64(0);
  household.name = statement.get_text(1);
  household.created_at = statement.get_text(2);
  household.updated_at = statement.get_text(3);
  return household;
}

// SELECT 列顺序，与 map_household 的下标一一对应，供 find_by_id/list 复用。
constexpr const char* kSelectColumns = "id, name, created_at, updated_at";

}  // namespace

// 插入家庭（预处理 + 参数绑定），返回自增主键。
std::int64_t HouseholdRepository::create(const Household& household) {
  Statement statement(database_,
                      "INSERT INTO household (name, created_at, updated_at) "
                      "VALUES (?, ?, ?);");
  statement.bind(1, household.name)
      .bind(2, household.created_at)
      .bind(3, household.updated_at)
      .run();
  return database_.last_insert_rowid();
}

std::optional<Household> HouseholdRepository::find_by_id(std::int64_t id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM household WHERE id = ?;");
  statement.bind(1, id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_household(statement);
}

std::vector<Household> HouseholdRepository::list() {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM household ORDER BY id ASC;");
  std::vector<Household> households;
  while (statement.step()) {
    households.push_back(map_household(statement));
  }
  return households;
}

bool HouseholdRepository::update(const Household& household) {
  Statement statement(database_,
                      "UPDATE household SET name = ?, updated_at = ? WHERE id = ?;");
  statement.bind(1, household.name)
      .bind(2, household.updated_at)
      .bind(3, household.id)
      .run();
  return database_.changes() > 0;
}

std::int64_t HouseholdRepository::count() {
  Statement statement(database_, "SELECT COUNT(*) FROM household;");
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

}  // namespace wt

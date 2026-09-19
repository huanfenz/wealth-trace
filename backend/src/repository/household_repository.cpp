#include "repository/household_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

Household map_household(Statement& statement) {
  Household household;
  household.id = statement.get_int64(0);
  household.name = statement.get_text(1);
  household.created_at = statement.get_text(2);
  household.updated_at = statement.get_text(3);
  return household;
}

constexpr const char* kSelectColumns = "id, name, created_at, updated_at";

}  // namespace

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

#include "repository/member_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {

HouseholdMember map_member(Statement& statement) {
  HouseholdMember member;
  member.id = statement.get_int64(0);
  member.household_id = statement.get_int64(1);
  member.name = statement.get_text(2);
  member.role = parse_member_role(statement.get_text(3)).value_or(MemberRole::Member);
  member.status =
      parse_member_status(statement.get_text(4)).value_or(MemberStatus::Active);
  member.created_at = statement.get_text(5);
  member.updated_at = statement.get_text(6);
  return member;
}

constexpr const char* kSelectColumns =
    "id, household_id, name, role, status, created_at, updated_at";

}  // namespace

std::int64_t MemberRepository::create(const HouseholdMember& member) {
  Statement statement(
      database_,
      "INSERT INTO household_member (household_id, name, role, status, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, ?);");
  statement.bind(1, member.household_id)
      .bind(2, member.name)
      .bind(3, std::string(to_string(member.role)))
      .bind(4, std::string(to_string(member.status)))
      .bind(5, member.created_at)
      .bind(6, member.updated_at)
      .run();
  return database_.last_insert_rowid();
}

std::optional<HouseholdMember> MemberRepository::find_by_id(std::int64_t id) {
  Statement statement(database_, std::string("SELECT ") + kSelectColumns +
                                     " FROM household_member WHERE id = ?;");
  statement.bind(1, id);
  if (!statement.step()) {
    return std::nullopt;
  }
  return map_member(statement);
}

std::vector<HouseholdMember> MemberRepository::list_by_household(
    std::int64_t household_id) {
  Statement statement(database_,
                      std::string("SELECT ") + kSelectColumns +
                          " FROM household_member WHERE household_id = ? "
                          "ORDER BY id ASC;");
  statement.bind(1, household_id);
  std::vector<HouseholdMember> members;
  while (statement.step()) {
    members.push_back(map_member(statement));
  }
  return members;
}

bool MemberRepository::update(const HouseholdMember& member) {
  Statement statement(database_,
                      "UPDATE household_member SET name = ?, role = ?, status = ?, "
                      "updated_at = ? WHERE id = ?;");
  statement.bind(1, member.name)
      .bind(2, std::string(to_string(member.role)))
      .bind(3, std::string(to_string(member.status)))
      .bind(4, member.updated_at)
      .bind(5, member.id)
      .run();
  return database_.changes() > 0;
}

bool MemberRepository::exists(std::int64_t id) {
  Statement statement(database_, "SELECT COUNT(*) FROM household_member WHERE id = ?;");
  statement.bind(1, id);
  if (statement.step()) {
    return statement.get_int64(0) > 0;
  }
  return false;
}

std::int64_t MemberRepository::count_by_household(std::int64_t household_id) {
  Statement statement(database_,
                      "SELECT COUNT(*) FROM household_member WHERE household_id = ?;");
  statement.bind(1, household_id);
  if (statement.step()) {
    return statement.get_int64(0);
  }
  return 0;
}

}  // namespace wt

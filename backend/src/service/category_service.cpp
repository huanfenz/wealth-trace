#include "service/category_service.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/statement.hpp"
#include "database/transaction.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {
TransactionCategory map_category(Statement& s) {
  TransactionCategory c;
  c.id = s.get_int64(0); c.household_id = s.get_int64(1);
  c.type = parse_transaction_type(s.get_text(2)).value_or(TransactionType::Expense);
  c.name = s.get_text(3); c.sort_order = static_cast<int>(s.get_int64(4));
  c.active = s.get_int64(5) != 0; c.created_at = s.get_text(6); c.updated_at = s.get_text(7);
  return c;
}
constexpr const char* kColumns = "id, household_id, type, name, sort_order, active, created_at, updated_at";
}  // namespace

void CategoryService::require_household(std::int64_t id) {
  Statement s(database_, "SELECT 1 FROM household WHERE id = ?;");
  s.bind(1, id);
  if (!s.step()) throw not_found("household not found");
}

void CategoryService::seed_defaults(std::int64_t household_id, TransactionType type) {
  Statement seeded(database_, "SELECT 1 FROM household_category_seed WHERE household_id = ? AND type = ?;");
  seeded.bind(1, household_id).bind(2, std::string(to_string(type)));
  if (seeded.step()) return;
  TransactionGuard tx(database_);
  const auto& names = type == TransactionType::Income ? defaults_.income : defaults_.expense;
  const auto now = time_util::now_iso8601();
  int order = 0;
  for (const auto& raw : names) {
    const auto name = strings::optional_text(raw, "name", 64);
    if (name.empty()) continue;
    Statement insert(database_, "INSERT OR IGNORE INTO transaction_category (household_id,type,name,sort_order,active,created_at,updated_at) VALUES (?,?,?,?,1,?,?);");
    insert.bind(1, household_id).bind(2, std::string(to_string(type))).bind(3, name)
        .bind(4, order++).bind(5, now).bind(6, now).run();
  }
  Statement mark(database_, "INSERT INTO household_category_seed (household_id, type) VALUES (?, ?);");
  mark.bind(1, household_id).bind(2, std::string(to_string(type))).run();
  tx.commit();
}

std::vector<TransactionCategory> CategoryService::list(std::int64_t household_id,
    TransactionType type, bool include_inactive) {
  std::scoped_lock lock(database_.mutex());
  require_household(household_id);
  seed_defaults(household_id, type);
  std::string sql = std::string("SELECT ") + kColumns + " FROM transaction_category WHERE household_id = ? AND type = ?";
  if (!include_inactive) sql += " AND active = 1";
  sql += " ORDER BY sort_order, id;";
  Statement s(database_, sql);
  s.bind(1, household_id).bind(2, std::string(to_string(type)));
  std::vector<TransactionCategory> result;
  while (s.step()) result.push_back(map_category(s));
  return result;
}

TransactionCategory CategoryService::create(std::int64_t household_id, TransactionType type,
                                              const std::string& raw_name) {
  std::scoped_lock lock(database_.mutex());
  require_household(household_id);
  if (type != TransactionType::Income && type != TransactionType::Expense)
    throw invalid_request("category type must be INCOME or EXPENSE");
  const auto name = strings::require_text(raw_name, "name", 64);
  const auto now = time_util::now_iso8601();
  Statement order(database_, "SELECT COALESCE(MAX(sort_order), -1) + 1 FROM transaction_category WHERE household_id = ? AND type = ?;");
  order.bind(1, household_id).bind(2, std::string(to_string(type)));
  const int next = order.step() ? static_cast<int>(order.get_int64(0)) : 0;
  try {
    Statement s(database_, "INSERT INTO transaction_category (household_id,type,name,sort_order,active,created_at,updated_at) VALUES (?,?,?,?,1,?,?);");
    s.bind(1, household_id).bind(2, std::string(to_string(type))).bind(3, name)
      .bind(4, next).bind(5, now).bind(6, now).run();
  } catch (const std::exception&) { throw conflict("category name already exists"); }
  const auto id = database_.last_insert_rowid();
  Statement get(database_, std::string("SELECT ") + kColumns + " FROM transaction_category WHERE id = ?;");
  get.bind(1, id); get.step(); return map_category(get);
}

TransactionCategory CategoryService::update(std::int64_t household_id, std::int64_t id, const std::string& raw_name,
                                              int sort_order) {
  std::scoped_lock lock(database_.mutex());
  if (sort_order < 0) throw invalid_request("sort_order must be non-negative");
  const auto name = strings::require_text(raw_name, "name", 64);
  {
    Statement exists(database_, "SELECT 1 FROM transaction_category WHERE id = ? AND household_id = ?;");
    exists.bind(1, id).bind(2, household_id);
    if (!exists.step()) throw not_found("category not found");
  }
  const auto now = time_util::now_iso8601();
  TransactionGuard tx(database_);
  try {
    Statement s(database_, "UPDATE transaction_category SET name = ?, sort_order = ?, updated_at = ? WHERE id = ?;");
    s.bind(1, name).bind(2, sort_order).bind(3, now).bind(4, id).run();
    Statement update_tx(database_, "UPDATE transactions SET category = ?, updated_at = ? WHERE category_id = ?;");
    update_tx.bind(1, name).bind(2, now).bind(3, id).run();
  } catch (const std::exception&) { throw conflict("category name already exists"); }
  tx.commit();
  Statement get(database_, std::string("SELECT ") + kColumns + " FROM transaction_category WHERE id = ?;");
  get.bind(1, id); get.step(); return map_category(get);
}

TransactionCategory CategoryService::set_active(std::int64_t household_id, std::int64_t id, bool active) {
  std::scoped_lock lock(database_.mutex());
  TransactionCategory c;
  {
    Statement s(database_, std::string("SELECT ") + kColumns + " FROM transaction_category WHERE id = ? AND household_id = ?;");
    s.bind(1, id).bind(2, household_id);
    if (!s.step()) throw not_found("category not found");
    c = map_category(s);
  }
  Statement update(database_, "UPDATE transaction_category SET active = ?, updated_at = ? WHERE id = ?;");
  const auto now = time_util::now_iso8601();
  update.bind(1, active ? 1 : 0).bind(2, now).bind(3, id).run();
  c.active = active; c.updated_at = now; return c;
}
}  // namespace wt

// system_state 仓储实现：基于 Prepared Statement 的键值读写。
#include "repository/system_state_repository.hpp"

#include <optional>
#include <string>
#include <string_view>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {

std::optional<std::string> SystemStateRepository::get(std::string_view key) {
  Statement statement(database_, "SELECT value FROM system_state WHERE key = ?;");
  statement.bind(1, key);
  if (!statement.step()) {
    return std::nullopt;
  }
  return statement.get_text(0);
}

void SystemStateRepository::set(std::string_view key, std::string_view value) {
  Statement statement(
      database_,
      "INSERT INTO system_state (key, value) VALUES (?, ?) "
      "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
  statement.bind(1, key).bind(2, value).run();
}

}  // namespace wt

#include "database/statement.hpp"

// 预编译语句实现：参数绑定、执行与列值读取。

#include <string>
#include <utility>

#include <sqlite3.h>

#include "common/error.hpp"
#include "database/database.hpp"

namespace wt {

Statement::Statement(Database& database, std::string_view sql)
    : database_(database.handle()), sql_(sql) {
  // 连接未打开时无法 prepare，直接报错。
  if (database_ == nullptr) {
    throw database_error("database is not open");
  }
  // 复制到 sql_ 以保证语句生命周期内 SQL 文本有效；-1 表示按 NUL 结尾读取全部文本。
  const int code = sqlite3_prepare_v2(database_, sql_.c_str(), -1, &stmt_, nullptr);
  if (code != SQLITE_OK) {
    throw_sqlite_error(database_, "failed to prepare statement");
  }
}

// RAII：析构时 finalize，释放语句占用的资源。
Statement::~Statement() {
  if (stmt_ != nullptr) {
    sqlite3_finalize(stmt_);
  }
}

// 移动构造：转移语句句柄，源置空以避免重复 finalize。
Statement::Statement(Statement&& other) noexcept
    : database_(other.database_), stmt_(other.stmt_), sql_(std::move(other.sql_)) {
  other.stmt_ = nullptr;
  other.database_ = nullptr;
}

// 移动赋值：先释放自身句柄，再转移源的句柄与状态。
Statement& Statement::operator=(Statement&& other) noexcept {
  if (this != &other) {
    if (stmt_ != nullptr) {
      sqlite3_finalize(stmt_);
    }
    database_ = other.database_;
    stmt_ = other.stmt_;
    sql_ = std::move(other.sql_);
    other.stmt_ = nullptr;
    other.database_ = nullptr;
  }
  return *this;
}

Statement& Statement::bind(int index, std::int64_t value) {
  // index 为 1-based，对应 SQL 中第 index 个 `?` 占位符。
  if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind integer");
  }
  return *this;
}

// int 统一提升为 int64 存储，避免平台字长差异。
Statement& Statement::bind(int index, int value) {
  return bind(index, static_cast<std::int64_t>(value));
}

// bool 以 0/1 整数形式持久化。
Statement& Statement::bind(int index, bool value) {
  return bind(index, static_cast<std::int64_t>(value ? 1 : 0));
}

Statement& Statement::bind(int index, const std::string& value) {
  return bind(index, std::string_view(value));
}

Statement& Statement::bind(int index, std::string_view value) {
  // SQLITE_TRANSIENT 让 SQLite 立即复制一份文本，因此 value 无需在语句执行期间保持有效。
  if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind text");
  }
  return *this;
}

Statement& Statement::bind_null(int index) {
  if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
    throw_sqlite_error(stmt_, "failed to bind null");
  }
  return *this;
}

// optional 有值则绑定值，无值则绑定 NULL。
Statement& Statement::bind_optional_int64(int index,
                                          const std::optional<std::int64_t>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

// 见 bind_optional_int64 的约定。
Statement& Statement::bind_optional_text(int index,
                                         const std::optional<std::string>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

// 见 bind_optional_int64 的约定。
Statement& Statement::bind_optional_bool(int index, const std::optional<bool>& value) {
  return value.has_value() ? bind(index, *value) : bind_null(index);
}

bool Statement::step() {
  const int code = sqlite3_step(stmt_);
  if (code == SQLITE_ROW) {
    return true;
  }
  if (code == SQLITE_DONE) {
    return false;
  }
  throw_sqlite_error(stmt_, "failed to execute statement");
}

void Statement::run() {
  // 反复 step 直到 SQLITE_DONE；中途出错时 step() 会抛异常。
  while (step()) {
  }
}

void Statement::reset() {
  // reset 后需清空绑定，避免语句复用时残留上次的参数。
  sqlite3_reset(stmt_);
  sqlite3_clear_bindings(stmt_);
}

bool Statement::is_null(int column) const {
  return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

std::int64_t Statement::get_int64(int column) const {
  return sqlite3_column_int64(stmt_, column);
}

int Statement::get_int(int column) const {
  return static_cast<int>(sqlite3_column_int64(stmt_, column));
}

bool Statement::get_bool(int column) const {
  // 与绑定约定一致：非 0 即为真。
  return sqlite3_column_int64(stmt_, column) != 0;
}

double Statement::get_double(int column) const {
  return sqlite3_column_double(stmt_, column);
}

std::string Statement::get_text(int column) const {
  const unsigned char* text = sqlite3_column_text(stmt_, column);
  // NULL 列返回空串；如需区分 NULL，请先用 is_null()/get_optional_text()。
  if (text == nullptr) {
    return {};
  }
  // 按实际字节数构造，不依赖 NUL 结尾（文本可能包含内嵌 NUL）。
  const int size = sqlite3_column_bytes(stmt_, column);
  return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
}

std::string Statement::get_text_or(int column, std::string fallback) const {
  return is_null(column) ? std::move(fallback) : get_text(column);
}

// 各 optional getter：列值为 NULL 时返回 nullopt，否则返回解析后的值。
std::optional<std::int64_t> Statement::get_optional_int64(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_int64(column);
}

// 见 get_optional_int64。
std::optional<std::string> Statement::get_optional_text(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_text(column);
}

// 见 get_optional_int64。
std::optional<bool> Statement::get_optional_bool(int column) const {
  if (is_null(column)) {
    return std::nullopt;
  }
  return get_bool(column);
}

}  // namespace wt

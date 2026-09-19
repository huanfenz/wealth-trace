#pragma once

// 预编译语句层：对 sqlite3_stmt 的 RAII 封装，统一参数绑定与列读取，防止 SQL 注入。
// 约定：绑定索引（bind 的 index）为 1-based，与 SQLite C API 一致；列索引（get/get_optional）为 0-based。

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace wt {

class Database;

// sqlite3_stmt 的 RAII 封装，析构时自动 finalize 释放语句资源。
// 绑定索引(1-based)与 SQLite API 保持一致；用户输入只能通过 bind() 传入，绝不拼接进 SQL。
class Statement {
 public:
  Statement(Database& database, std::string_view sql);
  ~Statement();

  // 拷贝会重复持有同一语句句柄，故禁用；移动通过转移句柄所有权实现。
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&& other) noexcept;
  Statement& operator=(Statement&& other) noexcept;

  Statement& bind(int index, std::int64_t value);
  Statement& bind(int index, int value);
  Statement& bind(int index, bool value);
  Statement& bind(int index, const std::string& value);
  Statement& bind(int index, std::string_view value);
  // 专门重载 const char*：避免字符串字面量经指针隐式退化为 bool，从而误选 bool 重载。
  Statement& bind(int index, const char* value) {
    return bind(index, std::string_view(value != nullptr ? value : ""));
  }
  Statement& bind_null(int index);
  Statement& bind_optional_int64(int index, const std::optional<std::int64_t>& value);
  Statement& bind_optional_text(int index, const std::optional<std::string>& value);
  Statement& bind_optional_bool(int index, const std::optional<bool>& value);

  // 前进一行。有可用行时返回 true；语句执行完毕（SQLITE_DONE）返回 false；出错抛异常。
  bool step();

  // 执行不返回行的语句（INSERT/UPDATE/DELETE），内部循环 step() 直到结束。
  void run();

  void reset();

  bool is_null(int column) const;
  std::int64_t get_int64(int column) const;
  int get_int(int column) const;
  bool get_bool(int column) const;
  double get_double(int column) const;
  std::string get_text(int column) const;
  std::string get_text_or(int column, std::string fallback) const;
  std::optional<std::int64_t> get_optional_int64(int column) const;
  std::optional<std::string> get_optional_text(int column) const;
  std::optional<bool> get_optional_bool(int column) const;

  sqlite3_stmt* handle() const noexcept { return stmt_; }

 private:
  sqlite3* database_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
  std::string sql_;
};

}  // namespace wt

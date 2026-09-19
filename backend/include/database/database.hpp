#pragma once

// 数据库连接层：提供 sqlite3 连接的 RAII 封装，以及统一的 SQLite -> ApiError 错误转换。
// 分层约定：位于 Repository 之下，只负责连接、PRAGMA 设置与错误映射。

#include <cstdint>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace wt {

// 将 `db` 上最近一次 SQLite 错误转换为对应的 ApiError 并抛出（[[noreturn]]，不会正常返回）。
// 映射规则：
//   SQLITE_CONSTRAINT           -> conflict()，HTTP 409 / 业务码 40901
//   SQLITE_BUSY / SQLITE_LOCKED -> ApiError(kDatabase, 503)，表示可重试的瞬时锁竞争
//   其他                        -> database_error()，HTTP 500 / 业务码 50001
// context 用于补充出错场景（如 "failed to bind text"），便于定位。
[[noreturn]] void throw_sqlite_error(sqlite3* db, std::string_view context);
// 语句重载：从语句句柄反查其所属连接，再复用上面的映射逻辑。
[[noreturn]] void throw_sqlite_error(sqlite3_stmt* stmt, std::string_view context);

// sqlite3 连接的轻量 RAII 封装：析构时自动关闭连接，支持移动、禁止拷贝。
class Database {
 public:
  Database() = default;
  ~Database();

  // 拷贝会重复持有同一连接句柄，故禁用；移动通过转移 handle_ 所有权实现。
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&& other) noexcept;
  Database& operator=(Database&& other) noexcept;

  // 打开（不存在则创建，并自动创建父目录）数据库文件，并应用必需的 PRAGMA。
  // 失败时抛异常。
  // path            数据库文件路径，":memory:" 表示内存库。
  // busy_timeout_ms 锁等待超时（毫秒），缓解并发写时的 SQLITE_BUSY。
  // wal             true 时启用 WAL 日志模式（读写并发更好）。
  // 启用的 PRAGMA：foreign_keys=ON、journal_mode=WAL、synchronous=NORMAL。
  void open(const std::string& path, int busy_timeout_ms = 5000, bool wal = true);
  void close();

  bool is_open() const noexcept { return handle_ != nullptr; }
  sqlite3* handle() const noexcept { return handle_; }

  // 执行一条或多条不带参数的 SQL，仅用于迁移与 DDL。
  // 注意：参数化查询必须走 Statement，切勿把用户输入拼接进 SQL。
  void exec(std::string_view sql);

  std::int64_t last_insert_rowid() const;
  int changes() const;

 private:
  sqlite3* handle_ = nullptr;
};

}  // namespace wt

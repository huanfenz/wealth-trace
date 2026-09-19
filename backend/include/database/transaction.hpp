#pragma once

// 事务层：TransactionGuard 用 RAII 管理事务边界，未提交即析构时自动回滚。

namespace wt {

class Database;

// RAII 事务：若未调用 commit() 就析构，会自动 ROLLBACK，避免异常路径下残留未完成事务。
// 同时支持 `BEGIN IMMEDIATE`（写事务，默认）与 `BEGIN DEFERRED`（只读事务）。
class TransactionGuard {
 public:
  explicit TransactionGuard(Database& database, bool immediate = true);
  ~TransactionGuard();

  // 事务守卫不可拷贝、不可移动：同一时刻只应由一个对象负责提交/回滚。
  TransactionGuard(const TransactionGuard&) = delete;
  TransactionGuard& operator=(const TransactionGuard&) = delete;
  TransactionGuard(TransactionGuard&&) = delete;
  TransactionGuard& operator=(TransactionGuard&&) = delete;

  void commit();
  void rollback();

  bool active() const noexcept { return active_; }

 private:
  Database* database_;
  bool active_ = false;
};

}  // namespace wt

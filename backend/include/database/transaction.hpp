#pragma once

namespace wt {

class Database;

// RAII transaction. Rolls back automatically if commit() was not called.
// Works for both `BEGIN IMMEDIATE` (write transactions, the default) and
// `BEGIN DEFERRED` (read only transactions).
class TransactionGuard {
 public:
  explicit TransactionGuard(Database& database, bool immediate = true);
  ~TransactionGuard();

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

#include "database/transaction.hpp"

#include "common/error.hpp"
#include "database/database.hpp"

namespace wt {

TransactionGuard::TransactionGuard(Database& database, bool immediate)
    : database_(&database) {
  database_->exec(immediate ? "BEGIN IMMEDIATE;" : "BEGIN DEFERRED;");
  active_ = true;
}

TransactionGuard::~TransactionGuard() {
  if (active_) {
    try {
      database_->exec("ROLLBACK;");
    } catch (...) {
      // Destructors must not throw; the original error is more relevant.
    }
  }
}

void TransactionGuard::commit() {
  if (!active_) {
    return;
  }
  database_->exec("COMMIT;");
  active_ = false;
}

void TransactionGuard::rollback() {
  if (!active_) {
    return;
  }
  database_->exec("ROLLBACK;");
  active_ = false;
}

}  // namespace wt

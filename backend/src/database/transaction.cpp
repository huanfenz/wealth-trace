#include "database/transaction.hpp"

// 事务实现：显式提交或回滚；未提交即析构时兜底回滚。

#include "common/error.hpp"
#include "database/database.hpp"

namespace wt {

TransactionGuard::TransactionGuard(Database& database, bool immediate)
    : database_(&database) {
  // IMMEDIATE 立即获取写锁，避免写事务中途升级锁时产生 SQLITE_BUSY；DEFERRED 用于只读场景。
  database_->exec(immediate ? "BEGIN IMMEDIATE;" : "BEGIN DEFERRED;");
  active_ = true;
}

TransactionGuard::~TransactionGuard() {
  // 未提交即析构：自动回滚，保证异常路径不会留下悬挂事务。
  if (active_) {
    try {
      database_->exec("ROLLBACK;");
    } catch (...) {
      // 析构函数不得抛异常；原始业务错误比回滚失败更值得保留，故此处吞掉异常。
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

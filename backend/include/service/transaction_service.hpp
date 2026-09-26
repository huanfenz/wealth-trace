// 交易服务：资产流水（Transaction）的记录与查询，负责余额增量维护与转账原子性。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "model/entities.hpp"
#include "repository/asset_repository.hpp"
#include "repository/transaction_repository.hpp"
#include "repository/recurring_investment_repository.hpp"

namespace wt {

class Database;

// 转账结果：同一个 transfer_group_id 下的两条配对流水。
struct TransferResult {
  // 转出方（TRANSFER_OUT，-amount）
  Transaction outgoing;
  // 转入方（TRANSFER_IN，+amount）
  Transaction incoming;
};

// 交易服务：一条 Transaction 只影响一个资产、只属于一个成员（属主取自资产）。
// amount 一律存正数绝对值，余额方向由 type 决定（ADJUSTMENT 例外，可正可负）。
// 记账时的 current_balance 变化会写入流水；删除流水时可选择保留余额。
// 写入流水与改余额在同一事务内完成。
class TransactionService {
 public:
  explicit TransactionService(Database& database)
      : assets_(database), transactions_(database), investments_(database), database_(database) {}

  // 记一笔收入：资产必须存在且 ACTIVE，amount 必须为正（>0）。
  // 业务错误：not_found / invalid_request / conflict（资产已关闭）。
  Transaction record_income(std::int64_t household_id, std::int64_t asset_id,
                            const std::optional<std::int64_t>& category_id,
                            std::int64_t amount, const std::string& transaction_time,
                            const std::optional<std::string>& remark);
  Transaction record_income(std::int64_t household_id, std::int64_t asset_id,
                            const std::string& legacy_category, std::int64_t amount,
                            const std::string& transaction_time,
                            const std::optional<std::string>& remark);

  // 记一笔支出：约束同收入，余额按 -amount 减少。
  Transaction record_expense(std::int64_t household_id, std::int64_t asset_id,
                             const std::optional<std::int64_t>& category_id,
                             std::int64_t amount, const std::string& transaction_time,
                             const std::optional<std::string>& remark);
  Transaction record_expense(std::int64_t household_id, std::int64_t asset_id,
                             const std::string& legacy_category, std::int64_t amount,
                             const std::string& transaction_time,
                             const std::optional<std::string>& remark);

  // 记一笔调整：用于对账纠偏，amount 可正可负（余额直接 +amount），
  // 但不允许为 0。资产同样必须存在且 ACTIVE。
  Transaction record_adjustment(std::int64_t household_id, std::int64_t asset_id,
                                std::int64_t amount,
                                const std::string& transaction_time,
                                const std::optional<std::string>& remark);

  // 转账：拆成 TRANSFER_OUT + TRANSFER_IN 两条流水，共用同一
  // transfer_group_id。要求金额为正、两端不同、同家庭且均 ACTIVE；
  // 在单个事务中写入两条流水并更新两端余额，四步全成功或全回滚。
  // 业务错误：invalid_request / not_found / conflict。
  TransferResult transfer(std::int64_t household_id, std::int64_t from_asset_id,
                          std::int64_t to_asset_id,
                          std::int64_t amount, const std::string& transaction_time,
                          const std::optional<std::string>& remark);

  // 分页查询流水；过滤条件由 TransactionQuery 给出。
  std::vector<Transaction> list(const TransactionQuery& query);
  // 统计满足条件的流水条数。
  std::int64_t count(const TransactionQuery& query);
  // 按 id 获取流水；不存在抛 not_found。
  Transaction get(std::int64_t id);
  // 仅修改收入/支出分类；nullopt 清空分类。分类须同家庭、同类型且启用。
  Transaction update_category(std::int64_t id, std::optional<std::int64_t> category_id);

  // 删除流水；rollback_assets 为 true 时回滚其对资产余额的影响。
  // 若该流水属于某次转账，则始终删除同组流水；回滚时分别调整各端余额。
  // 流水不存在抛 not_found。返回实际删除的底层流水条数（转账为 2，否则为 1）。
  std::int64_t remove(std::int64_t id, bool rollback_assets = true);

 private:
  // 单资产流水的公共实现：校验金额方向，检查资产 ACTIVE，计算 delta 与
  // 新余额，在事务内写流水并更新余额。收入/支出/调整都复用它。
  Transaction record(std::int64_t household_id, TransactionType type, std::int64_t asset_id,
                     const std::optional<std::int64_t>& category_id, std::int64_t amount,
                     const std::string& transaction_time,
                     const std::optional<std::string>& remark);

  AssetRepository assets_;
  TransactionRepository transactions_;
  RecurringInvestmentRepository investments_;
  Database& database_;
};

}  // namespace wt

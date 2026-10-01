#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "model/entities.hpp"
#include "repository/asset_repository.hpp"
#include "repository/transaction_repository.hpp"
#include "repository/recurring_investment_repository.hpp"

namespace wt {
class Database;
struct TransactionEntryInput { std::int64_t asset_id=0; TransactionDirection direction=TransactionDirection::In; std::int64_t amount=0; };
struct TransactionInput {
  TransactionType type=TransactionType::Expense;
  std::optional<std::int64_t> category_id;
  // 按名称解析/创建分类（字符串分类快捷路径）：在 write() 的事务内处理，
  // 与 category_id 互斥。避免分类 INSERT 与交易写入分裂成两个自动提交，
  // 失败时留下孤儿分类行。
  std::optional<std::string> category_name;
  bool preserve_legacy_category=false;
  std::optional<InvestmentAction> action;
  std::vector<TransactionEntryInput> entries;
  std::string transaction_time;
  std::optional<std::string> remark;
};
class TransactionService {
 public:
  explicit TransactionService(Database& db):assets_(db),transactions_(db),investments_(db),database_(db){}
  Transaction record_income(std::int64_t household,std::int64_t asset,const std::optional<std::int64_t>& category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction record_income(std::int64_t household,std::int64_t asset,const std::string& category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction record_expense(std::int64_t household,std::int64_t asset,const std::optional<std::int64_t>& category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction record_expense(std::int64_t household,std::int64_t asset,const std::string& category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction record_adjustment(std::int64_t household,std::int64_t asset,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction transfer(std::int64_t household,std::int64_t from_asset,std::int64_t to_asset,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction investment_buy(std::int64_t household,std::int64_t source_asset,std::int64_t target_asset,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  Transaction create(std::int64_t household,const TransactionInput& input);
  Transaction update(std::int64_t id,const TransactionInput& input,bool rollback_assets=true);
  std::vector<Transaction> list(const TransactionQuery& query);
  std::int64_t count(const TransactionQuery& query);
  Transaction get(std::int64_t id);
  TransactionDTO dto(std::int64_t id,bool include_entries=true);
  std::vector<TransactionDTO> list_dto(const TransactionQuery& query);
  std::vector<TransactionDTO> list_asset_dto(std::int64_t asset_id,const TransactionQuery& query);
  Transaction update_category(std::int64_t id,std::optional<std::int64_t> category_id);
  std::int64_t remove(std::int64_t id,bool rollback_assets=true);
 private:
  Transaction record_single(std::int64_t household,TransactionType type,std::int64_t asset,std::optional<std::int64_t> category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark);
  // 要求调用方已持有 database_.mutex()：按名称查找（仅 active）或创建分类，
  // 返回分类 id 并通过 name_out 输出规范化名称。
  std::int64_t resolve_category_locked(std::int64_t household,TransactionType type,const std::string& raw_name,std::optional<std::string>* name_out);
  Transaction write(std::int64_t household,const TransactionInput& input,std::optional<std::int64_t> id=std::nullopt,bool rollback_assets=true);
  AssetRepository assets_; TransactionRepository transactions_; RecurringInvestmentRepository investments_; Database& database_;
};
} // namespace wt

#include "repository/transaction_repository.hpp"

#include <string>
#include <utility>

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {
Transaction read_transaction(Statement& s) {
  Transaction t;
  t.id=s.get_int64(0); t.household_id=s.get_int64(1); t.owner_member_id=s.get_int64(2);
  t.type=parse_transaction_type(s.get_text(3)).value_or(TransactionType::Adjustment);
  t.category_id=s.get_optional_int64(4); t.category=s.get_optional_text(5);
  if (const auto action=s.get_optional_text(6); action) t.action=parse_investment_action(*action);
  t.transaction_time=s.get_text(7); t.remark=s.get_optional_text(8);
  t.status=parse_transaction_status(s.get_text(9)).value_or(TransactionStatus::Normal);
  t.created_at=s.get_text(10); t.updated_at=s.get_text(11); return t;
}
constexpr const char* kColumns =
  "id,household_id,owner_member_id,type,category_id,category,action,transaction_time,remark,status,created_at,updated_at";
std::string where_clause(const TransactionQuery& q, std::vector<std::string>& texts,
                         std::vector<std::int64_t>& ints) {
  std::string w=" WHERE t.household_id=?"; ints.push_back(q.household_id);
  if(q.owner_member_id){w+=" AND (t.owner_member_id=? OR EXISTS (SELECT 1 FROM transaction_entries oe WHERE oe.transaction_id=t.id AND oe.owner_member_id=?))";ints.push_back(*q.owner_member_id);ints.push_back(*q.owner_member_id);}
  if(q.asset_id){w+=" AND EXISTS (SELECT 1 FROM transaction_entries e WHERE e.transaction_id=t.id AND e.asset_id=?)";ints.push_back(*q.asset_id);}
  if(q.category_id){w+=" AND t.category_id=?";ints.push_back(*q.category_id);}
  if(q.type){w+=" AND t.type=?";texts.emplace_back(to_string(*q.type));}
  if(q.from_time){w+=" AND t.transaction_time>=?";texts.push_back(*q.from_time);}
  if(q.to_time){w+=" AND t.transaction_time<=?";texts.push_back(*q.to_time);}
  return w;
}
void bind_query(Statement& s,const std::vector<std::string>& texts,const std::vector<std::int64_t>& ints){
  int i=1; for(auto v:ints)s.bind(i++,v); for(const auto& v:texts)s.bind(i++,v);
}
}

std::int64_t TransactionRepository::create(const Transaction& t) {
  Statement s(database_,"INSERT INTO transactions (household_id,owner_member_id,type,category_id,category,action,transaction_time,remark,status,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?);");
  s.bind(1,t.household_id).bind(2,t.owner_member_id).bind(3,std::string(to_string(t.type)))
   .bind_optional_int64(4,t.category_id).bind_optional_text(5,t.category)
   .bind_optional_text(6,t.action?std::optional<std::string>(std::string(to_string(*t.action))):std::nullopt)
   .bind(7,t.transaction_time).bind_optional_text(8,t.remark).bind(9,std::string(to_string(t.status)))
   .bind(10,t.created_at).bind(11,t.updated_at).run(); return database_.last_insert_rowid();
}
std::optional<Transaction> TransactionRepository::find_by_id(std::int64_t id) {
  Statement s(database_,std::string("SELECT ")+kColumns+" FROM transactions WHERE id=?;");
  s.bind(1,id); if(!s.step())return std::nullopt; return read_transaction(s);
}
bool TransactionRepository::update(const Transaction& t) {
  Statement s(database_,"UPDATE transactions SET owner_member_id=?,type=?,category_id=?,category=?,action=?,transaction_time=?,remark=?,updated_at=? WHERE id=?;");
  s.bind(1,t.owner_member_id).bind(2,std::string(to_string(t.type))).bind_optional_int64(3,t.category_id)
   .bind_optional_text(4,t.category)
   .bind_optional_text(5,t.action?std::optional<std::string>(std::string(to_string(*t.action))):std::nullopt)
   .bind(6,t.transaction_time).bind_optional_text(7,t.remark).bind(8,t.updated_at).bind(9,t.id).run();
  return database_.changes()>0;
}
bool TransactionRepository::update_category(std::int64_t id,
    std::optional<std::int64_t> category_id,const std::optional<std::string>& category,
    const std::string& updated_at) {
  Statement s(database_,"UPDATE transactions SET category_id=?,category=?,updated_at=? WHERE id=?;");
  s.bind_optional_int64(1,category_id).bind_optional_text(2,category)
   .bind(3,updated_at).bind(4,id).run();
  return database_.changes()>0;
}
bool TransactionRepository::remove(std::int64_t id) {
  Statement s(database_,"DELETE FROM transactions WHERE id=?;");s.bind(1,id).run();return database_.changes()>0;
}
std::vector<Transaction> TransactionRepository::list(const TransactionQuery& q) {
  std::vector<std::string> texts;std::vector<std::int64_t> ints;
  const auto w=where_clause(q,texts,ints);
  Statement s(database_,std::string("SELECT ")+kColumns+" FROM transactions t"+w+" ORDER BY t.transaction_time DESC,t.id DESC LIMIT ? OFFSET ?;");
  bind_query(s,texts,ints);int i=static_cast<int>(ints.size()+texts.size()+1);s.bind(i,q.limit);++i;s.bind(i,q.offset);
  std::vector<Transaction> out;while(s.step())out.push_back(read_transaction(s));return out;
}
std::int64_t TransactionRepository::count(const TransactionQuery& q) {
  std::vector<std::string> texts;std::vector<std::int64_t> ints;
  const auto w=where_clause(q,texts,ints);Statement s(database_,"SELECT COUNT(*) FROM transactions t"+w+";");
  bind_query(s,texts,ints);return s.step()?s.get_int64(0):0;
}
std::vector<TransactionEntry> TransactionRepository::entries(std::int64_t id) {
  Statement s(database_,"SELECT id,transaction_id,household_id,owner_member_id,asset_id,direction,amount,balance_before,balance_after,created_at FROM transaction_entries WHERE transaction_id=? ORDER BY id;");
  s.bind(1,id);std::vector<TransactionEntry> out;while(s.step()){
    TransactionEntry e;e.id=s.get_int64(0);e.transaction_id=s.get_int64(1);e.household_id=s.get_int64(2);
    e.owner_member_id=s.get_int64(3);e.asset_id=s.get_int64(4);e.direction=parse_transaction_direction(s.get_text(5)).value_or(TransactionDirection::In);
    e.amount=s.get_int64(6);e.balance_before=s.get_optional_int64(7);e.balance_after=s.get_optional_int64(8);e.created_at=s.get_text(9);out.push_back(e);
  }return out;
}
std::int64_t TransactionRepository::add_entry(const TransactionEntry& e) {
  Statement s(database_,"INSERT INTO transaction_entries (transaction_id,household_id,owner_member_id,asset_id,direction,amount,balance_before,balance_after,created_at) VALUES (?,?,?,?,?,?,?,?,?);");
  s.bind(1,e.transaction_id).bind(2,e.household_id).bind(3,e.owner_member_id).bind(4,e.asset_id)
   .bind(5,std::string(to_string(e.direction))).bind(6,e.amount).bind_optional_int64(7,e.balance_before)
   .bind_optional_int64(8,e.balance_after).bind(9,e.created_at).run();return database_.last_insert_rowid();
}
void TransactionRepository::clear_entries(std::int64_t id){Statement s(database_,"DELETE FROM transaction_entries WHERE transaction_id=?;");s.bind(1,id).run();}
void TransactionRepository::add_investment_detail(std::int64_t tx,std::int64_t asset,InvestmentAction action,std::optional<std::int64_t> principal){
  Statement s(database_,"INSERT INTO investment_transaction_details (transaction_id,asset_id,action,principal) VALUES (?,?,?,?);");
  s.bind(1,tx).bind(2,asset).bind(3,std::string(to_string(action))).bind_optional_int64(4,principal).run();
}
std::optional<std::pair<std::int64_t,InvestmentAction>> TransactionRepository::investment_detail(std::int64_t tx){
  Statement s(database_,"SELECT asset_id,action FROM investment_transaction_details WHERE transaction_id=?;");s.bind(1,tx);
  if(!s.step())return std::nullopt;return std::pair{s.get_int64(0),parse_investment_action(s.get_text(1)).value_or(InvestmentAction::Buy)};
}
std::int64_t TransactionRepository::count_by_asset(std::int64_t asset){Statement s(database_,"SELECT COUNT(*) FROM transaction_entries WHERE asset_id=?;");s.bind(1,asset);return s.step()?s.get_int64(0):0;}
std::optional<std::pair<std::string,std::string>> TransactionRepository::asset_summary(std::int64_t asset){Statement s(database_,"SELECT name,CAST(owner_member_id AS TEXT) FROM asset WHERE id=?;");s.bind(1,asset);if(!s.step())return std::nullopt;return std::pair{s.get_text(0),s.get_text(1)};}
}  // namespace wt

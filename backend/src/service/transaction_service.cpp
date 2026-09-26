#include "service/transaction_service.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <set>
#include "common/error.hpp"
#include "database/database.hpp"
#include "database/statement.hpp"
#include "database/transaction.hpp"
#include "utils/strings.hpp"
#include "utils/time_util.hpp"
#include "utils/flexible_term.hpp"
#include "utils/commercial_pension.hpp"

namespace wt { namespace {
std::string tx_time(const std::string& value){return strings::is_blank(value)?time_util::now_iso8601():time_util::require_datetime(value,"transaction_time");}
std::optional<std::string> clean_remark(const std::optional<std::string>& value){if(!value)return std::nullopt;auto v=strings::optional_text(*value,"remark",500);if(v.empty())return std::nullopt;return v;}
std::int64_t magnitude(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min())
    throw invalid_request("amount is outside the supported range");
  return value < 0 ? -value : value;
}
std::int64_t checked_add(std::int64_t left,std::int64_t right) {
  if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
      (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right))
    throw invalid_request("asset balance is outside the supported range");
  return left + right;
}
}

Transaction TransactionService::record_single(std::int64_t household,TransactionType type,std::int64_t asset,
 std::optional<std::int64_t> category,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark){
  if(type!=TransactionType::Adjustment&&amount<=0)throw invalid_request("amount must be positive");
  if(amount==0)throw invalid_request("amount must not be zero");
  TransactionInput in;in.type=type;in.category_id=category;in.transaction_time=time;in.remark=remark;
  const auto direction=(type==TransactionType::Income|| (type==TransactionType::Adjustment&&amount>0))?TransactionDirection::In:TransactionDirection::Out;
  in.entries.push_back({asset,direction,magnitude(amount)});return create(household,in);
}
Transaction TransactionService::record_income(std::int64_t h,std::int64_t a,const std::optional<std::int64_t>& c,std::int64_t n,const std::string& t,const std::optional<std::string>& r){return record_single(h,TransactionType::Income,a,c,n,t,r);}
Transaction TransactionService::record_income(std::int64_t h,std::int64_t a,const std::string& c,std::int64_t n,const std::string& t,const std::optional<std::string>& r){std::optional<std::int64_t> id;if(!strings::is_blank(c)){std::scoped_lock l(database_.mutex());Statement s(database_,"SELECT id FROM transaction_category WHERE household_id=? AND type='INCOME' AND name=? AND active=1;");s.bind(1,h).bind(2,strings::require_text(c,"category",64));if(s.step())id=s.get_int64(0);else{const auto now=time_util::now_iso8601();Statement add(database_,"INSERT INTO transaction_category(household_id,type,name,sort_order,active,created_at,updated_at) VALUES(?,'INCOME',?,0,1,?,?);");add.bind(1,h).bind(2,c).bind(3,now).bind(4,now).run();id=database_.last_insert_rowid();}}return record_income(h,a,id,n,t,r);}
Transaction TransactionService::record_expense(std::int64_t h,std::int64_t a,const std::optional<std::int64_t>& c,std::int64_t n,const std::string& t,const std::optional<std::string>& r){return record_single(h,TransactionType::Expense,a,c,n,t,r);}
Transaction TransactionService::record_expense(std::int64_t h,std::int64_t a,const std::string& c,std::int64_t n,const std::string& t,const std::optional<std::string>& r){std::optional<std::int64_t> id;if(!strings::is_blank(c)){std::scoped_lock l(database_.mutex());Statement s(database_,"SELECT id FROM transaction_category WHERE household_id=? AND type='EXPENSE' AND name=? AND active=1;");s.bind(1,h).bind(2,strings::require_text(c,"category",64));if(s.step())id=s.get_int64(0);else{const auto now=time_util::now_iso8601();Statement add(database_,"INSERT INTO transaction_category(household_id,type,name,sort_order,active,created_at,updated_at) VALUES(?,'EXPENSE',?,0,1,?,?);");add.bind(1,h).bind(2,c).bind(3,now).bind(4,now).run();id=database_.last_insert_rowid();}}return record_expense(h,a,id,n,t,r);}
Transaction TransactionService::record_adjustment(std::int64_t h,std::int64_t a,std::int64_t n,const std::string& t,const std::optional<std::string>& r){return record_single(h,TransactionType::Adjustment,a,std::nullopt,n,t,r);}
Transaction TransactionService::transfer(std::int64_t h,std::int64_t from,std::int64_t to,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark){TransactionInput in;in.type=TransactionType::Transfer;in.transaction_time=time;in.remark=remark;in.entries={{from,TransactionDirection::Out,amount},{to,TransactionDirection::In,amount}};return create(h,in);}
Transaction TransactionService::investment_buy(std::int64_t h,std::int64_t from,std::int64_t to,std::int64_t amount,const std::string& time,const std::optional<std::string>& remark){TransactionInput in;in.type=TransactionType::Investment;in.action=InvestmentAction::Buy;in.transaction_time=time;in.remark=remark;in.entries={{from,TransactionDirection::Out,amount},{to,TransactionDirection::In,amount}};return create(h,in);}

Transaction TransactionService::create(std::int64_t h,const TransactionInput& in){std::scoped_lock l(database_.mutex());return write(h,in);}
Transaction TransactionService::write(std::int64_t household,const TransactionInput& input,std::optional<std::int64_t> id){
  if(input.entries.empty())throw invalid_request("transaction must contain entries");
  if(input.type==TransactionType::Income&&(input.entries.size()!=1||input.entries[0].direction!=TransactionDirection::In))throw invalid_request("income requires one IN entry");
  if(input.type==TransactionType::Expense&&(input.entries.size()!=1||input.entries[0].direction!=TransactionDirection::Out))throw invalid_request("expense requires one OUT entry");
  if(input.type==TransactionType::Adjustment&&input.entries.size()!=1)throw invalid_request("adjustment requires one entry");
  if(input.type==TransactionType::Transfer||input.type==TransactionType::Investment){
    if(input.entries.size()!=2)throw invalid_request("transfer and investment require two entries");
    const auto incoming=std::find_if(input.entries.begin(),input.entries.end(),[](const auto&e){return e.direction==TransactionDirection::In;});
    const auto outgoing=std::find_if(input.entries.begin(),input.entries.end(),[](const auto&e){return e.direction==TransactionDirection::Out;});
    if(incoming==input.entries.end()||outgoing==input.entries.end()||
       incoming->amount!=outgoing->amount||incoming->asset_id==outgoing->asset_id)
      throw invalid_request("transfer entries must balance across distinct assets");
    if(input.type==TransactionType::Investment&&input.action!=InvestmentAction::Buy)throw invalid_request("only investment BUY is supported");
  }
  if(input.type!=TransactionType::Income&&input.type!=TransactionType::Expense&&input.category_id)throw invalid_request("category is only valid for income or expense");
  if((input.type==TransactionType::Income||input.type==TransactionType::Expense)&&input.action)throw invalid_request("action is only valid for investment");
  if(input.type!=TransactionType::Investment&&input.action)throw invalid_request("action is only valid for investment");
  Transaction tx; if(id){auto old=transactions_.find_by_id(*id);if(!old)throw not_found("transaction not found");tx=*old;}
  tx.household_id=household;tx.type=input.type;tx.category_id=input.category_id;tx.action=input.action;
  tx.transaction_time=tx_time(input.transaction_time);tx.remark=clean_remark(input.remark);tx.updated_at=time_util::now_iso8601();
  if(input.category_id){Statement c(database_,"SELECT name,household_id,type,active FROM transaction_category WHERE id=?;");c.bind(1,*input.category_id);if(!c.step())throw not_found("category not found");if(c.get_int64(1)!=household||c.get_text(2)!=to_string(input.type))throw invalid_request("category does not match household and transaction type");if(!c.get_int64(3))throw conflict("category is inactive");tx.category=c.get_text(0);}else tx.category.reset();
  std::vector<Asset> selected;selected.reserve(input.entries.size());
  for(const auto& e:input.entries){if(e.amount<=0)throw invalid_request("entry amount must be positive");auto a=assets_.find_by_id(e.asset_id);if(!a)throw not_found("asset not found");if(a->household_id!=household)throw invalid_request("asset does not belong to household");if(a->status!=AssetStatus::Active)throw conflict("asset is closed");selected.push_back(*a);}
  tx.owner_member_id=selected.front().owner_member_id;
  if(input.type==TransactionType::Investment){const auto out=std::find_if(input.entries.begin(),input.entries.end(),[](const auto& e){return e.direction==TransactionDirection::Out;});const auto in=std::find_if(input.entries.begin(),input.entries.end(),[](const auto& e){return e.direction==TransactionDirection::In;});if(out==input.entries.end()||in==input.entries.end())throw invalid_request("BUY requires a source OUT and destination IN entry");const auto si=static_cast<std::size_t>(out-input.entries.begin()),di=static_cast<std::size_t>(in-input.entries.begin());if(selected[si].current_balance<out->amount)throw conflict("source asset balance is insufficient");if(selected[di].asset_type==AssetType::Liability)throw invalid_request("investment target cannot be a liability");}
  if(input.type==TransactionType::Transfer||input.type==TransactionType::Investment){
    const auto out=std::find_if(input.entries.begin(),input.entries.end(),[](const auto&e){return e.direction==TransactionDirection::Out;});
    const auto& source=selected[static_cast<std::size_t>(out-input.entries.begin())];
    if(source.asset_type==AssetType::FlexibleTerm){const auto d=assets_.find_flexible_term_detail(out->asset_id);if(!d||!flexible_term::can_transfer(*d,time_util::business_today()))throw conflict("flexible term asset is not open for transfer today");}
    if(source.asset_type==AssetType::CommercialPension){const auto d=assets_.find_commercial_pension_detail(out->asset_id);const auto now=time_util::utc_to_business(time_util::now_iso8601());if(!d||!d->redeem_at_maturity||!d->redeem_at||now<*d->redeem_at)throw conflict("commercial pension is not yet available for redemption");}
  }
  TransactionGuard guard(database_);const auto now=tx.updated_at;
  if(id){for(const auto& old:transactions_.entries(*id)){auto a=assets_.find_by_id(old.asset_id);if(a)assets_.update_balance(a->id,checked_add(a->current_balance,-entry_delta(old.direction,old.amount)),now);}transactions_.clear_entries(*id);Statement detail(database_,"DELETE FROM investment_transaction_details WHERE transaction_id=?;");detail.bind(1,*id).run();transactions_.update(tx);}
  else{tx.owner_member_id=selected.front().owner_member_id;tx.created_at=now;tx.status=TransactionStatus::Normal;tx.id=transactions_.create(tx);}
  for(std::size_t i=0;i<input.entries.size();++i){const auto& e=input.entries[i];auto a=assets_.find_by_id(e.asset_id);const auto before=a->current_balance;const auto after=checked_add(before,entry_delta(e.direction,e.amount));assets_.update_balance(a->id,after,now);TransactionEntry row;row.transaction_id=tx.id;row.household_id=household;row.owner_member_id=a->owner_member_id;row.asset_id=a->id;row.direction=e.direction;row.amount=e.amount;row.balance_before=before;row.balance_after=after;row.created_at=now;transactions_.add_entry(row);}
  if(input.type==TransactionType::Investment){const auto target=std::find_if(input.entries.begin(),input.entries.end(),[](const auto&e){return e.direction==TransactionDirection::In;});transactions_.add_investment_detail(tx.id,target->asset_id,*input.action,target->amount);}
  guard.commit();return tx;
}
std::vector<Transaction> TransactionService::list(const TransactionQuery&q){std::scoped_lock l(database_.mutex());return transactions_.list(q);}
std::int64_t TransactionService::count(const TransactionQuery&q){std::scoped_lock l(database_.mutex());return transactions_.count(q);}
Transaction TransactionService::get(std::int64_t id){std::scoped_lock l(database_.mutex());auto t=transactions_.find_by_id(id);if(!t)throw not_found("transaction not found");return *t;}
std::vector<TransactionDTO> TransactionService::list_dto(const TransactionQuery&q){std::scoped_lock l(database_.mutex());std::vector<TransactionDTO> out;for(const auto&t:transactions_.list(q))out.push_back(dto(t.id,false));return out;}
std::vector<TransactionDTO> TransactionService::list_asset_dto(std::int64_t asset,const TransactionQuery&q){std::scoped_lock l(database_.mutex());auto current=assets_.find_by_id(asset);if(!current)throw not_found("asset not found");if(current->household_id!=q.household_id)throw invalid_request("asset does not belong to household");auto query=q;query.asset_id=asset;std::vector<TransactionDTO> out;for(const auto&t:transactions_.list(query)){auto d=dto(t.id,true);const auto own=std::find_if(d.entries.begin(),d.entries.end(),[&](const auto&e){return e.asset_id==asset;});if(own==d.entries.end())continue;d.amount=own->amount;d.direction=own->direction==TransactionDirection::In?DisplayDirection::In:DisplayDirection::Out;for(const auto&e:d.entries)if(e.asset_id!=asset){auto other=assets_.find_by_id(e.asset_id);if(other){d.subtitle=e.direction==TransactionDirection::In?"转至"+other->name:other->name+"转入";break;}}out.push_back(std::move(d));}return out;}
TransactionDTO TransactionService::dto(std::int64_t id,bool include_entries){std::scoped_lock l(database_.mutex());auto tx=transactions_.find_by_id(id);if(!tx)throw not_found("transaction not found");TransactionDTO d;d.transaction=*tx;auto entries=transactions_.entries(id);if(include_entries)d.entries=entries;
  if(tx->type==TransactionType::Income||tx->type==TransactionType::Expense||tx->type==TransactionType::Adjustment){if(entries.empty())throw conflict("transaction has no entries");d.amount=entries.front().amount;d.direction=entries.front().direction==TransactionDirection::In?DisplayDirection::In:DisplayDirection::Out;auto a=assets_.find_by_id(entries.front().asset_id);if(a){d.subtitle=a->name;if(entries.front().direction==TransactionDirection::Out){d.source_asset_id=a->id;d.source_asset_name=a->name;}else{d.destination_asset_id=a->id;d.destination_asset_name=a->name;}}d.title=tx->type==TransactionType::Adjustment?"余额调整":tx->category.value_or(tx->type==TransactionType::Income?"收入":"支出");}
  else{d.direction=DisplayDirection::Neutral;auto in=std::find_if(entries.begin(),entries.end(),[](const auto&e){return e.direction==TransactionDirection::In;});auto out=std::find_if(entries.begin(),entries.end(),[](const auto&e){return e.direction==TransactionDirection::Out;});if(out!=entries.end()){d.amount=out->amount;auto a=assets_.find_by_id(out->asset_id);if(a){d.source_asset_id=a->id;d.source_asset_name=a->name;}}if(in!=entries.end()){d.amount=std::max(d.amount,in->amount);auto a=assets_.find_by_id(in->asset_id);if(a){d.destination_asset_id=a->id;d.destination_asset_name=a->name;}}if(d.source_asset_name&&d.destination_asset_name)d.subtitle=*d.source_asset_name+" → "+*d.destination_asset_name;d.title=tx->type==TransactionType::Transfer?"转账":(tx->action==InvestmentAction::Buy?"买入":"投资");}
  return d;}
Transaction TransactionService::update(std::int64_t id,const TransactionInput& input){
  std::scoped_lock l(database_.mutex());auto old=get(id);
  Statement linked(database_,"SELECT 1 FROM recurring_investment_execution WHERE transaction_id=? LIMIT 1;");
  linked.bind(1,id);
  if(linked.step())throw conflict("linked recurring investment transactions cannot be edited");
  return write(old.household_id,input,id);
}
Transaction TransactionService::update_category(std::int64_t id,std::optional<std::int64_t> c){
  std::scoped_lock l(database_.mutex());auto tx=get(id);
  if(tx.type!=TransactionType::Income&&tx.type!=TransactionType::Expense)throw invalid_request("only income and expense transactions can have categories");
  std::optional<std::string> name;
  if(c){
    Statement category(database_,"SELECT name,household_id,type,active FROM transaction_category WHERE id=?;");
    category.bind(1,*c);
    if(!category.step())throw not_found("category not found");
    if(category.get_int64(1)!=tx.household_id||category.get_text(2)!=to_string(tx.type))throw invalid_request("category does not match household and transaction type");
    if(!category.get_int64(3))throw conflict("category is inactive");
    name=category.get_text(0);
  }
  TransactionGuard guard(database_);
  transactions_.update_category(id,c,name,time_util::now_iso8601());
  guard.commit();return get(id);
}
std::int64_t TransactionService::remove(std::int64_t id,bool rollback){std::scoped_lock l(database_.mutex());auto tx=transactions_.find_by_id(id);if(!tx)throw not_found("transaction not found");const auto es=transactions_.entries(id);TransactionGuard guard(database_);const auto now=time_util::now_iso8601();if(rollback){for(const auto&e:es){auto a=assets_.find_by_id(e.asset_id);if(a)assets_.update_balance(a->id,checked_add(a->current_balance,-entry_delta(e.direction,e.amount)),now);}investments_.mark_reversed(id,now);}transactions_.remove(id);guard.commit();return 1;}
} // namespace wt

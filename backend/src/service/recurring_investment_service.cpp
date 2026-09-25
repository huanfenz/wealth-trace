#include "service/recurring_investment_service.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>

#include "common/error.hpp"
#include "database/database.hpp"
#include "database/transaction.hpp"
#include "model/enums.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {
using namespace std::chrono;

bool valid_calendar_date(const std::string& text) {
  if (!time_util::is_valid_date(text)) return false;
  const int y=std::stoi(text.substr(0,4)), m=std::stoi(text.substr(5,2)), d=std::stoi(text.substr(8,2));
  return year_month_day{year{y},month{static_cast<unsigned>(m)},day{static_cast<unsigned>(d)}}.ok();
}
sys_days parse_day(const std::string& text) {
  return sys_days{year_month_day{year{std::stoi(text.substr(0,4))},
      month{static_cast<unsigned>(std::stoi(text.substr(5,2)))},
      day{static_cast<unsigned>(std::stoi(text.substr(8,2)))}}};
}
std::string format_day(sys_days value) {
  const year_month_day ymd{value};
  char buffer[32];
  std::snprintf(buffer,sizeof(buffer),"%04d-%02u-%02u",int(ymd.year()),unsigned(ymd.month()),unsigned(ymd.day()));
  return buffer;
}
std::string add_month(const std::string& date,int day_of_month) {
  year_month_day ymd{parse_day(date)};
  const year_month_day next{(ymd.year()/ymd.month()+months{1})/day{static_cast<unsigned>(day_of_month)}};
  return format_day(sys_days{next});
}
int weekday_iso(const std::string& date) {
  return weekday{parse_day(date)}.iso_encoding();
}
std::string first_occurrence(const std::string& start,const std::string& frequency,
                             const std::optional<int>& weekday_value,
                             const std::optional<int>& month_day) {
  if (frequency=="DAILY") return start;
  if (frequency=="WEEKLY" || frequency=="BIWEEKLY") {
    const int delta=(*weekday_value-weekday_iso(start)+7)%7;
    return time_util::add_days(start,delta);
  }
  const int day_value=*month_day;
  if (std::stoi(start.substr(8,2))<=day_value) {
    return start.substr(0,8)+(day_value<10?"0":"")+std::to_string(day_value);
  }
  return add_month(start,day_value);
}
std::string next_occurrence(const RecurringInvestmentPlan& p,const std::string& current) {
  if(p.frequency=="DAILY") return time_util::add_days(current,1);
  if(p.frequency=="WEEKLY") return time_util::add_days(current,7);
  if(p.frequency=="BIWEEKLY") return time_util::add_days(current,14);
  return add_month(current,*p.month_day);
}
}

std::vector<RecurringInvestmentPlan> RecurringInvestmentService::list(std::int64_t household_id) {
  std::scoped_lock lock(database_.mutex()); return plans_.list(household_id);
}
std::vector<RecurringInvestmentExecution> RecurringInvestmentService::executions(std::int64_t id) {
  std::scoped_lock lock(database_.mutex());
  const auto plan=require_plan(id);
  return plans_.list_executions(plan.id);
}

void RecurringInvestmentService::validate_assets(std::int64_t household_id,
    const RecurringInvestmentInput& input,std::int64_t* owner_member_id) {
  if(input.amount<=0) throw invalid_request("amount must be positive");
  const auto target=assets_.find_by_id(input.target_asset_id);
  const auto source=assets_.find_by_id(input.source_asset_id);
  if(!target || !source) throw not_found("investment asset not found");
  if(target->household_id!=household_id || source->household_id!=household_id)
    throw invalid_request("investment assets must belong to household");
  if(target->id==source->id) throw invalid_request("payment asset must differ from target asset");
  if(target->asset_type!=AssetType::StockFund) throw invalid_request("target asset must be a stock fund");
  if(source->asset_type==AssetType::Liability) throw invalid_request("payment asset cannot be a liability");
  if(target->status!=AssetStatus::Active || source->status!=AssetStatus::Active)
    throw conflict("investment assets must be active");
  if(target->owner_member_id!=source->owner_member_id)
    throw invalid_request("investment assets must belong to the same member");
  *owner_member_id=target->owner_member_id;
  if(input.frequency!="DAILY" && input.frequency!="WEEKLY" && input.frequency!="BIWEEKLY" && input.frequency!="MONTHLY")
    throw invalid_request("invalid investment frequency");
  if((input.frequency=="WEEKLY" || input.frequency=="BIWEEKLY") &&
     (!input.weekday || *input.weekday<1 || *input.weekday>7))
    throw invalid_request("weekday must be between 1 and 7");
  if(input.frequency!="WEEKLY" && input.frequency!="BIWEEKLY" && input.weekday)
    throw invalid_request("weekday is only valid for weekly frequencies");
  if(input.frequency=="MONTHLY" && (!input.month_day || *input.month_day<1 || *input.month_day>28))
    throw invalid_request("month_day must be between 1 and 28");
  if(input.frequency!="MONTHLY" && input.month_day)
    throw invalid_request("month_day is only valid for monthly frequency");
  if(!valid_calendar_date(input.start_date)) throw invalid_request("start_date must be a valid date");
}

RecurringInvestmentPlan RecurringInvestmentService::normalize(std::int64_t household_id,
    std::int64_t owner_member_id,const RecurringInvestmentInput& input,const std::string& first_date) const {
  RecurringInvestmentPlan p;
  p.household_id=household_id; p.owner_member_id=owner_member_id;
  p.target_asset_id=input.target_asset_id; p.source_asset_id=input.source_asset_id;
  p.amount=input.amount; p.frequency=input.frequency; p.weekday=input.weekday;
  p.month_day=input.month_day; p.start_date=input.start_date; p.next_due_date=first_date;
  p.status="ACTIVE"; return p;
}

RecurringInvestmentPlan RecurringInvestmentService::create(std::int64_t household_id,
    const RecurringInvestmentInput& input) {
  std::scoped_lock lock(database_.mutex());
  const std::string today=time_util::business_today();
  std::int64_t owner=0; validate_assets(household_id,input,&owner);
  if(input.start_date<today) throw invalid_request("start_date cannot be in the past");
  auto p=normalize(household_id,owner,input,first_occurrence(input.start_date,input.frequency,input.weekday,input.month_day));
  const auto now=time_util::now_iso8601(); p.created_at=now; p.updated_at=now;
  TransactionGuard tx(database_); p.id=plans_.create(p); tx.commit();
  process_due();
  return *plans_.find(p.id);
}

RecurringInvestmentPlan RecurringInvestmentService::require_plan(std::int64_t id) {
  auto p=plans_.find(id); if(!p) throw not_found("recurring investment plan not found"); return *p;
}

RecurringInvestmentPlan RecurringInvestmentService::update(std::int64_t id,const RecurringInvestmentInput& input) {
  std::scoped_lock lock(database_.mutex());
  auto p=require_plan(id); if(p.status=="DELETED") throw conflict("plan is deleted");
  std::int64_t owner=0; validate_assets(p.household_id,input,&owner);
  if(owner!=p.owner_member_id) throw invalid_request("plan assets must remain with the same member");
  const std::string today=time_util::business_today();
  p.target_asset_id=input.target_asset_id; p.source_asset_id=input.source_asset_id;
  p.amount=input.amount; p.frequency=input.frequency; p.weekday=input.weekday; p.month_day=input.month_day;
  p.start_date=input.start_date;
  p.next_due_date=first_occurrence(input.start_date,input.frequency,input.weekday,input.month_day);
  while(p.next_due_date<today) p.next_due_date=next_occurrence(p,p.next_due_date);
  p.updated_at=time_util::now_iso8601();
  TransactionGuard tx(database_); plans_.update(p); tx.commit();
  process_due(); return *plans_.find(id);
}

RecurringInvestmentPlan RecurringInvestmentService::set_status(std::int64_t id,const std::string& status) {
  std::scoped_lock lock(database_.mutex()); auto p=require_plan(id);
  if(p.status=="DELETED") throw conflict("plan is deleted");
  if(status!="ACTIVE" && status!="PAUSED") throw invalid_request("status must be ACTIVE or PAUSED");
  if(status=="ACTIVE" && p.status=="PAUSED") {
    const auto today=time_util::business_today();
    while(p.next_due_date<today) p.next_due_date=next_occurrence(p,p.next_due_date);
  }
  TransactionGuard tx(database_);
  if(status=="ACTIVE") plans_.update(p);
  plans_.set_status(id,status,time_util::now_iso8601()); tx.commit();
  if(status=="ACTIVE") process_due();
  return require_plan(id);
}

void RecurringInvestmentService::remove(std::int64_t id) {
  std::scoped_lock lock(database_.mutex()); auto p=require_plan(id);
  if(p.status=="DELETED") return;
  TransactionGuard tx(database_); plans_.set_status(id,"DELETED",time_util::now_iso8601()); tx.commit();
}

std::int64_t RecurringInvestmentService::process_due() {
  std::scoped_lock lock(database_.mutex());
  const std::string today=time_util::business_today();
  std::int64_t count=0;
  for(const auto& p:plans_.list_due(today)) { process_plan(p,today); ++count; }
  return count;
}

void RecurringInvestmentService::process_plan(RecurringInvestmentPlan p,const std::string& today) {
  std::size_t guard=0;
  while(p.next_due_date<=today) {
    if(++guard>100000) throw invalid_request("investment schedule is too long to catch up");
    const std::string due=p.next_due_date;
    const std::string next=next_occurrence(p,due);
    if(plans_.has_execution(p.id,due)) {
      plans_.update_due(p.id,next,time_util::now_iso8601()); p.next_due_date=next; continue;
    }
    RecurringInvestmentExecution e;
    e.plan_id=p.id; e.scheduled_date=due; e.amount=p.amount;
    e.source_asset_id=p.source_asset_id; e.target_asset_id=p.target_asset_id;
    e.status="FAILED"; e.failure_reason="execution failed";
    execute(e,next); p.next_due_date=next;
  }
}

void RecurringInvestmentService::execute(RecurringInvestmentExecution& e,
                                          const std::optional<std::string>& next_due) {
  const auto now=time_util::now_iso8601(); e.updated_at=now;
  TransactionGuard tx(database_);
  if(e.id==0) e.created_at=now;
  e.status="FAILED"; e.transfer_group_id.reset(); e.failure_reason.reset();
  std::optional<Asset> source, target;
  if(e.source_asset_id) source=assets_.find_by_id(*e.source_asset_id);
  if(e.target_asset_id) target=assets_.find_by_id(*e.target_asset_id);
  if(!source || !target) e.failure_reason="付款资产或股票基金已删除";
  else if(source->status!=AssetStatus::Active || target->status!=AssetStatus::Active)
    e.failure_reason="付款资产或股票基金已关闭";
  else if(source->asset_type==AssetType::Liability || target->asset_type!=AssetType::StockFund ||
          source->household_id!=target->household_id || source->owner_member_id!=target->owner_member_id)
    e.failure_reason="定投资产关系已失效";
  else if(source->current_balance<e.amount) e.failure_reason="付款资产余额不足";
  else {
    const auto group=transactions_.next_transfer_group_id();
    const auto source_after=source->current_balance-e.amount;
    const auto target_after=target->current_balance+e.amount;
    const auto date=time_util::business_to_utc(e.scheduled_date+" 00:00:00");
    Transaction out;
    out.household_id=source->household_id; out.owner_member_id=source->owner_member_id;
    out.asset_id=source->id; out.type=TransactionType::TransferOut; out.amount=e.amount;
    out.transfer_group_id=group; out.balance_before=source->current_balance; out.balance_after=source_after;
    out.transaction_time=date; out.remark="股票基金定投"; out.status=TransactionStatus::Normal;
    out.created_at=now; out.updated_at=now;
    Transaction in;
    in.household_id=target->household_id; in.owner_member_id=target->owner_member_id;
    in.asset_id=target->id; in.type=TransactionType::TransferIn; in.amount=e.amount;
    in.transfer_group_id=group; in.balance_before=target->current_balance; in.balance_after=target_after;
    in.transaction_time=date; in.remark="股票基金定投"; in.status=TransactionStatus::Normal;
    in.created_at=now; in.updated_at=now;
    transactions_.create(out); assets_.update_balance(source->id,source_after,now);
    transactions_.create(in); assets_.update_balance(target->id,target_after,now);
    e.status="SUCCESS"; e.transfer_group_id=group;
  }
  if(e.id==0) e.id=plans_.create_execution(e);
  else plans_.update_execution(e);
  if(next_due) plans_.update_due(e.plan_id,*next_due,now);
  tx.commit();
}

RecurringInvestmentExecution RecurringInvestmentService::retry(std::int64_t execution_id) {
  std::scoped_lock lock(database_.mutex());
  auto e=plans_.find_execution(execution_id); if(!e) throw not_found("investment execution not found");
  const auto p=require_plan(e->plan_id);
  if(p.status=="DELETED") throw conflict("plan is deleted");
  if(e->status!="FAILED") throw conflict("only failed executions can be retried");
  execute(*e); return *plans_.find_execution(execution_id);
}

RecurringInvestmentExecution RecurringInvestmentService::execute_now(std::int64_t plan_id) {
  std::scoped_lock lock(database_.mutex());
  const auto plan = require_plan(plan_id);
  if (plan.status == "DELETED") throw conflict("plan is deleted");
  const auto today = time_util::business_today();
  if (plans_.has_execution(plan.id, today)) {
    throw conflict("plan already has an execution today");
  }
  RecurringInvestmentExecution execution;
  execution.plan_id = plan.id;
  execution.scheduled_date = today;
  execution.amount = plan.amount;
  execution.source_asset_id = plan.source_asset_id;
  execution.target_asset_id = plan.target_asset_id;
  execute(execution);
  return *plans_.find_execution(execution.id);
}
}  // namespace wt

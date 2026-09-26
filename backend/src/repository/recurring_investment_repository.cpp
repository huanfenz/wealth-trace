#include "repository/recurring_investment_repository.hpp"

#include "database/database.hpp"
#include "database/statement.hpp"

namespace wt {
namespace {
constexpr const char* kPlanColumns =
    "id, household_id, owner_member_id, target_asset_id, source_asset_id, amount, "
    "frequency, weekday, month_day, start_date, next_due_date, status, created_at, updated_at";
constexpr const char* kExecutionColumns =
    "id, plan_id, scheduled_date, amount, source_asset_id, target_asset_id, status, "
    "transaction_id, failure_reason, created_at, updated_at";

RecurringInvestmentPlan map_plan(Statement& s) {
  RecurringInvestmentPlan p;
  p.id=s.get_int64(0); p.household_id=s.get_int64(1); p.owner_member_id=s.get_int64(2);
  p.target_asset_id=s.get_optional_int64(3); p.source_asset_id=s.get_optional_int64(4);
  p.amount=s.get_int64(5); p.frequency=s.get_text(6);
  if (!s.is_null(7)) p.weekday=s.get_int(7);
  if (!s.is_null(8)) p.month_day=s.get_int(8);
  p.start_date=s.get_text(9); p.next_due_date=s.get_text(10); p.status=s.get_text(11);
  p.created_at=s.get_text(12); p.updated_at=s.get_text(13);
  return p;
}
RecurringInvestmentExecution map_execution(Statement& s) {
  RecurringInvestmentExecution e;
  e.id=s.get_int64(0); e.plan_id=s.get_int64(1); e.scheduled_date=s.get_text(2);
  e.amount=s.get_int64(3); e.source_asset_id=s.get_optional_int64(4);
  e.target_asset_id=s.get_optional_int64(5); e.status=s.get_text(6);
  e.transaction_id=s.get_optional_int64(7); e.failure_reason=s.get_optional_text(8);
  e.created_at=s.get_text(9); e.updated_at=s.get_text(10);
  return e;
}
}  // namespace

std::int64_t RecurringInvestmentRepository::create(const RecurringInvestmentPlan& p) {
  Statement s(database_, "INSERT INTO recurring_investment_plan "
      "(household_id,owner_member_id,target_asset_id,source_asset_id,amount,frequency,weekday,month_day,start_date,next_due_date,status,created_at,updated_at) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?);");
  s.bind(1,p.household_id).bind(2,p.owner_member_id).bind_optional_int64(3,p.target_asset_id)
   .bind_optional_int64(4,p.source_asset_id).bind(5,p.amount).bind(6,p.frequency)
   .bind_optional_int64(7,p.weekday ? std::optional<std::int64_t>(*p.weekday) : std::nullopt)
   .bind_optional_int64(8,p.month_day ? std::optional<std::int64_t>(*p.month_day) : std::nullopt)
   .bind(9,p.start_date).bind(10,p.next_due_date).bind(11,p.status).bind(12,p.created_at).bind(13,p.updated_at).run();
  return database_.last_insert_rowid();
}
std::optional<RecurringInvestmentPlan> RecurringInvestmentRepository::find(std::int64_t id) {
  Statement s(database_, std::string("SELECT ")+kPlanColumns+" FROM recurring_investment_plan WHERE id=?;");
  s.bind(1,id); if(!s.step()) return std::nullopt; return map_plan(s);
}
std::vector<RecurringInvestmentPlan> RecurringInvestmentRepository::list(std::int64_t household_id) {
  Statement s(database_, std::string("SELECT ")+kPlanColumns+" FROM recurring_investment_plan WHERE household_id=? ORDER BY status='DELETED', id DESC;");
  s.bind(1,household_id); std::vector<RecurringInvestmentPlan> out;
  while (s.step()) {
    out.push_back(map_plan(s));
  }
  return out;
}
std::vector<RecurringInvestmentPlan> RecurringInvestmentRepository::list_due(const std::string& today) {
  Statement s(database_, std::string("SELECT ")+kPlanColumns+" FROM recurring_investment_plan WHERE status='ACTIVE' AND next_due_date<=? ORDER BY next_due_date,id;");
  s.bind(1,today); std::vector<RecurringInvestmentPlan> out;
  while (s.step()) {
    out.push_back(map_plan(s));
  }
  return out;
}
bool RecurringInvestmentRepository::has_execution(std::int64_t id,const std::string& date) {
  Statement s(database_,"SELECT 1 FROM recurring_investment_execution WHERE plan_id=? AND scheduled_date=? LIMIT 1;");
  s.bind(1,id).bind(2,date); return s.step();
}
bool RecurringInvestmentRepository::update(const RecurringInvestmentPlan& p) {
  Statement s(database_, "UPDATE recurring_investment_plan SET target_asset_id=?,source_asset_id=?,amount=?,frequency=?,weekday=?,month_day=?,start_date=?,next_due_date=?,updated_at=? WHERE id=? AND status!='DELETED';");
  s.bind_optional_int64(1,p.target_asset_id).bind_optional_int64(2,p.source_asset_id).bind(3,p.amount)
   .bind(4,p.frequency).bind_optional_int64(5,p.weekday ? std::optional<std::int64_t>(*p.weekday) : std::nullopt)
   .bind_optional_int64(6,p.month_day ? std::optional<std::int64_t>(*p.month_day) : std::nullopt)
   .bind(7,p.start_date).bind(8,p.next_due_date).bind(9,p.updated_at).bind(10,p.id).run();
  return database_.changes()>0;
}
bool RecurringInvestmentRepository::update_due(std::int64_t id,const std::string& date,const std::string& now) {
  Statement s(database_,"UPDATE recurring_investment_plan SET next_due_date=?,updated_at=? WHERE id=? AND status='ACTIVE';");
  s.bind(1,date).bind(2,now).bind(3,id).run(); return database_.changes()>0;
}
bool RecurringInvestmentRepository::set_status(std::int64_t id,const std::string& status,const std::string& now) {
  Statement s(database_,"UPDATE recurring_investment_plan SET status=?,updated_at=? WHERE id=? AND status!='DELETED';");
  s.bind(1,status).bind(2,now).bind(3,id).run(); return database_.changes()>0;
}
std::int64_t RecurringInvestmentRepository::create_execution(const RecurringInvestmentExecution& e) {
  Statement s(database_,"INSERT INTO recurring_investment_execution (plan_id,scheduled_date,amount,source_asset_id,target_asset_id,status,transaction_id,failure_reason,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?);");
  s.bind(1,e.plan_id).bind(2,e.scheduled_date).bind(3,e.amount).bind_optional_int64(4,e.source_asset_id)
   .bind_optional_int64(5,e.target_asset_id).bind(6,e.status).bind_optional_int64(7,e.transaction_id)
   .bind_optional_text(8,e.failure_reason).bind(9,e.created_at).bind(10,e.updated_at).run(); return database_.last_insert_rowid();
}
std::optional<RecurringInvestmentExecution> RecurringInvestmentRepository::find_execution(std::int64_t id) {
  Statement s(database_,std::string("SELECT ")+kExecutionColumns+" FROM recurring_investment_execution WHERE id=?;");
  s.bind(1,id); if(!s.step()) return std::nullopt; return map_execution(s);
}
std::vector<RecurringInvestmentExecution> RecurringInvestmentRepository::list_executions(std::int64_t id) {
  Statement s(database_,std::string("SELECT ")+kExecutionColumns+" FROM recurring_investment_execution WHERE plan_id=? ORDER BY scheduled_date DESC,id DESC;");
  s.bind(1,id); std::vector<RecurringInvestmentExecution> out;
  while (s.step()) {
    out.push_back(map_execution(s));
  }
  return out;
}
bool RecurringInvestmentRepository::update_execution(const RecurringInvestmentExecution& e) {
  Statement s(database_,"UPDATE recurring_investment_execution SET status=?,transaction_id=?,failure_reason=?,updated_at=? WHERE id=?;");
  s.bind(1,e.status).bind_optional_int64(2,e.transaction_id).bind_optional_text(3,e.failure_reason)
   .bind(4,e.updated_at).bind(5,e.id).run(); return database_.changes()>0;
}
bool RecurringInvestmentRepository::mark_reversed(std::int64_t transaction,const std::string& now) {
  Statement s(database_,"UPDATE recurring_investment_execution SET status='REVERSED',updated_at=? WHERE transaction_id=? AND status='SUCCESS';");
  s.bind(1,now).bind(2,transaction).run(); return database_.changes()>0;
}
}  // namespace wt

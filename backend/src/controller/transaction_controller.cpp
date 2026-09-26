#include "controller/transaction_controller.hpp"
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"
#include "model/enums.hpp"

namespace wt { namespace {
std::optional<std::string> body_time(const nlohmann::json& b){return dto::optional_string(b,"transaction_time",19);}
std::optional<std::string> body_remark(const nlohmann::json& b){return dto::optional_string(b,"remark",500);}
TransactionInput transaction_input(const nlohmann::json& body){
  TransactionInput in;const auto type=parse_transaction_type(dto::require_string(body,"type",32));if(!type)throw invalid_request("invalid transaction type");in.type=*type;
  in.category_id=dto::optional_int64(body,"category_id");in.transaction_time=body_time(body).value_or("");in.remark=body_remark(body);
  if(const auto a=dto::optional_string(body,"action",32);a){in.action=parse_investment_action(*a);if(!in.action)throw invalid_request("invalid investment action");}
  if(!body.contains("entries")||!body["entries"].is_array())throw invalid_request("entries must be an array");
  for(const auto& row:body["entries"]){if(!row.is_object())throw invalid_request("entry must be an object");const auto dir=parse_transaction_direction(dto::require_string(row,"direction",8));if(!dir)throw invalid_request("invalid entry direction");in.entries.push_back({dto::require_int64(row,"asset_id"),*dir,dto::require_int64(row,"amount")});}
  return in;
}
nlohmann::json list_query(const crow::request& request,int household,TransactionService& service){
  TransactionQuery q;q.household_id=household;q.owner_member_id=http::query_int64(request,"owner_member_id");q.asset_id=http::query_int64(request,"asset_id");
  if(const auto type=http::query_string(request,"type");type){q.type=parse_transaction_type(*type);if(!q.type)throw invalid_request("invalid transaction type");}
  q.from_time=http::query_string(request,"from");q.to_time=http::query_string(request,"to");q.limit=http::query_int(request,"limit",200);q.offset=http::query_int(request,"offset",0);
  if(q.limit<=0||q.limit>1000||q.offset<0)throw invalid_request("invalid pagination");nlohmann::json items=nlohmann::json::array();for(const auto& t:service.list_dto(q))items.push_back(dto::to_json(t));return {{"total",service.count(q)},{"items",items}};
}
}

void TransactionController::register_routes(crow::SimpleApp& app){
 CROW_ROUTE(app,"/api/households/<int>/transactions").methods("GET"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{return list_query(r,h,service_);});});
 CROW_ROUTE(app,"/api/households/<int>/transactions").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto in=transaction_input(dto::parse_object(r.body));return dto::to_json(service_.dto(service_.create(h,in).id));});});
 CROW_ROUTE(app,"/api/households/<int>/transactions/income").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto b=dto::parse_object(r.body);auto id=dto::require_int64(b,"asset_id");auto amount=dto::require_int64(b,"amount");auto c=dto::optional_int64(b,"category_id");Transaction t;if(c)t=service_.record_income(h,id,c,amount,body_time(b).value_or(""),body_remark(b));else t=service_.record_income(h,id,dto::optional_string(b,"category",64).value_or(""),amount,body_time(b).value_or(""),body_remark(b));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/households/<int>/transactions/expense").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto b=dto::parse_object(r.body);auto id=dto::require_int64(b,"asset_id");auto amount=dto::require_int64(b,"amount");auto c=dto::optional_int64(b,"category_id");Transaction t;if(c)t=service_.record_expense(h,id,c,amount,body_time(b).value_or(""),body_remark(b));else t=service_.record_expense(h,id,dto::optional_string(b,"category",64).value_or(""),amount,body_time(b).value_or(""),body_remark(b));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/households/<int>/transactions/adjustment").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto b=dto::parse_object(r.body);auto t=service_.record_adjustment(h,dto::require_int64(b,"asset_id"),dto::require_int64(b,"amount"),body_time(b).value_or(""),body_remark(b));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/households/<int>/transfers").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto b=dto::parse_object(r.body);auto t=service_.transfer(h,dto::require_int64(b,"from_asset_id"),dto::require_int64(b,"to_asset_id"),dto::require_int64(b,"amount"),body_time(b).value_or(""),body_remark(b));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/households/<int>/investments/buy").methods("POST"_method)([this](const crow::request&r,int h){return http::handle([this,&r,h]{auto b=dto::parse_object(r.body);auto t=service_.investment_buy(h,dto::require_int64(b,"from_asset_id"),dto::require_int64(b,"to_asset_id"),dto::require_int64(b,"amount"),body_time(b).value_or(""),body_remark(b));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/assets/<int>/transactions").methods("GET"_method)([this](const crow::request&r,int asset){return http::handle([this,&r,asset]{const auto h=http::query_int64(r,"household_id");if(!h)throw invalid_request("household_id is required");TransactionQuery q;q.household_id=*h;q.owner_member_id=http::query_int64(r,"owner_member_id");q.from_time=http::query_string(r,"from");q.to_time=http::query_string(r,"to");q.limit=http::query_int(r,"limit",200);q.offset=http::query_int(r,"offset",0);if(const auto type=http::query_string(r,"type");type){q.type=parse_transaction_type(*type);if(!q.type)throw invalid_request("invalid transaction type");}if(q.limit<=0||q.limit>1000||q.offset<0)throw invalid_request("invalid pagination");auto rows=service_.list_asset_dto(asset,q);q.asset_id=asset;nlohmann::json result=nlohmann::json::array();for(const auto& t:rows)result.push_back(dto::to_json(t));return nlohmann::json{{"total",service_.count(q)},{"items",result}};});});
 CROW_ROUTE(app,"/api/transactions/<int>").methods("GET"_method)([this](int id){return http::handle([this,id]{return dto::to_json(service_.dto(id));});});
 CROW_ROUTE(app,"/api/transactions/<int>").methods("PUT"_method)([this](const crow::request&r,int id){return http::handle([this,&r,id]{auto t=service_.update(id,transaction_input(dto::parse_object(r.body)));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/transactions/<int>/category").methods("PUT"_method)([this](const crow::request&r,int id){return http::handle([this,&r,id]{auto b=dto::parse_object(r.body);if(!b.contains("category_id"))throw invalid_request("category_id is required");auto t=service_.update_category(id,dto::optional_int64(b,"category_id"));return dto::to_json(service_.dto(t.id));});});
 CROW_ROUTE(app,"/api/transactions/<int>").methods("DELETE"_method)([this](const crow::request&r,int id){return http::handle([this,&r,id]{auto rollback=http::query_string(r,"rollback_assets");if(rollback&&*rollback!="true"&&*rollback!="false")throw invalid_request("rollback_assets must be true or false");return nlohmann::json{{"deleted",service_.remove(id,!rollback||*rollback=="true")}};});});
}
} // namespace wt

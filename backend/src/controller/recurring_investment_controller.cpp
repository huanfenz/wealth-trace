#include "controller/recurring_investment_controller.hpp"

#include <nlohmann/json.hpp>
#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"

namespace wt {
namespace {
RecurringInvestmentInput parse_input(const nlohmann::json& body) {
  RecurringInvestmentInput input;
  input.target_asset_id=dto::require_int64(body,"target_asset_id");
  input.source_asset_id=dto::require_int64(body,"source_asset_id");
  input.amount=dto::require_int64(body,"amount");
  input.frequency=dto::require_string(body,"frequency",16);
  input.start_date=dto::require_string(body,"start_date",10);
  if(const auto value=dto::optional_int64(body,"weekday")) input.weekday=static_cast<int>(*value);
  if(const auto value=dto::optional_int64(body,"month_day")) input.month_day=static_cast<int>(*value);
  return input;
}
}
void RecurringInvestmentController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app,"/api/households/<int>/investment-plans").methods("GET"_method)(
      [this](int household_id){ return http::handle([this,household_id]{
        nlohmann::json rows=nlohmann::json::array();
        for(const auto& p:service_.list(household_id)) rows.push_back(dto::to_json(p));
        return rows;
      }); });
  CROW_ROUTE(app,"/api/households/<int>/investment-plans").methods("POST"_method)(
      [this](const crow::request& req,int household_id){ return http::handle([this,&req,household_id]{
        return dto::to_json(service_.create(household_id,parse_input(dto::parse_object(req.body))));
      }); });
  CROW_ROUTE(app,"/api/investment-plans/<int>").methods("PUT"_method)(
      [this](const crow::request& req,int id){ return http::handle([this,&req,id]{
        return dto::to_json(service_.update(id,parse_input(dto::parse_object(req.body))));
      }); });
  CROW_ROUTE(app,"/api/investment-plans/<int>/status").methods("PUT"_method)(
      [this](const crow::request& req,int id){ return http::handle([this,&req,id]{
        const auto body=dto::parse_object(req.body);
        return dto::to_json(service_.set_status(id,dto::require_string(body,"status",16)));
      }); });
  CROW_ROUTE(app,"/api/investment-plans/<int>").methods("DELETE"_method)(
      [this](int id){ return http::handle([this,id]{ service_.remove(id); return nlohmann::json{{"deleted",true}}; }); });
  CROW_ROUTE(app,"/api/investment-plans/<int>/executions").methods("GET"_method)(
      [this](int id){ return http::handle([this,id]{
        nlohmann::json rows=nlohmann::json::array();
        for(const auto& e:service_.executions(id)) {
          rows.push_back(dto::to_json(e));
        }
        return rows;
      }); });
  CROW_ROUTE(app,"/api/investment-executions/<int>/retry").methods("POST"_method)(
      [this](int id){ return http::handle([this,id]{ return dto::to_json(service_.retry(id)); }); });
  CROW_ROUTE(app,"/api/investment-plans/<int>/execute").methods("POST"_method)(
      [this](int id){ return http::handle([this,id]{ return dto::to_json(service_.execute_now(id)); }); });
}
}

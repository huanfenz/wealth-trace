#include "controller/asset_controller.hpp"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "controller/http_util.hpp"
#include "database/database.hpp"
#include "dto/json_helpers.hpp"
#include "dto/serialization.hpp"
#include "model/enums.hpp"
#include "utils/time_util.hpp"

namespace wt {
namespace {

std::optional<std::string> parse_date_field(const nlohmann::json& object, const char* key,
                                            std::size_t max_length) {
  const auto value = dto::optional_string(object, key, max_length);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return time_util::require_date(*value, key);
}

std::optional<TermDepositDetail> parse_term_deposit(const nlohmann::json& body) {
  if (!body.contains("term_deposit") || body.at("term_deposit").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("term_deposit");
  if (!object.is_object()) {
    throw invalid_request("term_deposit must be an object");
  }
  TermDepositDetail detail;
  detail.principal = dto::optional_int64(object, "principal").value_or(0);
  detail.annual_interest_rate =
      dto::optional_int64(object, "annual_interest_rate").value_or(0);
  detail.start_date = parse_date_field(object, "start_date", 10);
  detail.maturity_date = parse_date_field(object, "maturity_date", 10);
  detail.term_value = dto::optional_int64(object, "term_value");
  if (const auto unit = dto::optional_string(object, "term_unit", 16); unit.has_value()) {
    const auto parsed = parse_term_unit(*unit);
    if (!parsed.has_value()) {
      throw invalid_request("term_unit must be DAY, MONTH or YEAR");
    }
    detail.term_unit = *parsed;
  }
  detail.interest_type = dto::optional_string(object, "interest_type", 32);
  detail.auto_rollover = dto::optional_bool(object, "auto_rollover", false);
  detail.maturity_action = dto::optional_string(object, "maturity_action", 32);
  return detail;
}

std::optional<FundDetail> parse_fund(const nlohmann::json& body) {
  if (!body.contains("fund") || body.at("fund").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("fund");
  if (!object.is_object()) {
    throw invalid_request("fund must be an object");
  }
  FundDetail detail;
  detail.fund_code = dto::optional_string(object, "fund_code", 32);
  detail.fund_name = dto::optional_string(object, "fund_name", 100);
  detail.fund_type = dto::optional_string(object, "fund_type", 32);
  detail.lock_start_date = parse_date_field(object, "lock_start_date", 10);
  detail.lock_end_date = parse_date_field(object, "lock_end_date", 10);
  return detail;
}

std::optional<BondDetail> parse_bond(const nlohmann::json& body) {
  if (!body.contains("bond") || body.at("bond").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("bond");
  if (!object.is_object()) {
    throw invalid_request("bond must be an object");
  }
  BondDetail detail;
  detail.bond_code = dto::optional_string(object, "bond_code", 32);
  detail.bond_name = dto::optional_string(object, "bond_name", 100);
  detail.principal = dto::optional_int64(object, "principal").value_or(0);
  detail.annual_coupon_rate =
      dto::optional_int64(object, "annual_coupon_rate").value_or(0);
  detail.purchase_date = parse_date_field(object, "purchase_date", 10);
  detail.maturity_date = parse_date_field(object, "maturity_date", 10);
  detail.lock_end_date = parse_date_field(object, "lock_end_date", 10);
  return detail;
}

std::optional<InsuranceDetail> parse_insurance(const nlohmann::json& body) {
  if (!body.contains("insurance") || body.at("insurance").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("insurance");
  if (!object.is_object()) {
    throw invalid_request("insurance must be an object");
  }
  InsuranceDetail detail;
  detail.policy_no = dto::optional_string(object, "policy_no", 64);
  detail.insurance_company = dto::optional_string(object, "insurance_company", 100);
  detail.product_name = dto::optional_string(object, "product_name", 100);
  detail.insurance_type = dto::optional_string(object, "insurance_type", 32);
  detail.effective_date = parse_date_field(object, "effective_date", 10);
  detail.maturity_date = parse_date_field(object, "maturity_date", 10);
  detail.annual_premium = dto::optional_int64(object, "annual_premium").value_or(0);
  detail.total_paid_premium =
      dto::optional_int64(object, "total_paid_premium").value_or(0);
  detail.insured_amount = dto::optional_int64(object, "insured_amount").value_or(0);
  detail.payment_years = dto::optional_int64(object, "payment_years");
  return detail;
}

AssetType require_asset_type(const nlohmann::json& body) {
  const auto value = dto::require_string(body, "asset_type", 32);
  const auto parsed = parse_asset_type(value);
  if (!parsed.has_value()) {
    throw invalid_request("invalid asset_type");
  }
  return *parsed;
}

}  // namespace

void AssetController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/households/<int>/assets").methods("GET"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto owner = http::query_int64(request, "owner_member_id");
          const auto account = http::query_int64(request, "account_id");
          nlohmann::json data = nlohmann::json::array();
          for (const auto& bundle : service_.list_bundles(id, owner, account)) {
            data.push_back(dto::to_json(bundle));
          }
          return data;
        });
      });

  CROW_ROUTE(app, "/api/households/<int>/assets").methods("POST"_method)(
      [this](const crow::request& request, int /*id*/) {
        return http::handle([this, &request] {
          const auto body = dto::parse_object(request.body);
          AssetCreateInput input;
          input.account_id = dto::require_int64(body, "account_id");
          input.name = dto::require_string(body, "name", 100);
          input.asset_type = require_asset_type(body);
          input.opening_balance = dto::optional_int64(body, "opening_balance").value_or(0);
          input.remark = dto::optional_string(body, "remark", 500);
          input.term_deposit = parse_term_deposit(body);
          input.fund = parse_fund(body);
          input.bond = parse_bond(body);
          input.insurance = parse_insurance(body);
          return dto::to_json(service_.create(input));
        });
      });

  CROW_ROUTE(app, "/api/assets/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get_bundle(id)); });
  });

  CROW_ROUTE(app, "/api/assets/<int>").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto existing = service_.get(id);
          const auto name = dto::optional_string_or(body, "name", existing.name);
          const auto opening = dto::optional_int64(body, "opening_balance");
          const auto remark = body.contains("remark")
                                  ? dto::optional_string(body, "remark", 500)
                                  : existing.remark;
          return dto::to_json(service_.update_metadata(id, name, opening, remark));
        });
      });

  CROW_ROUTE(app, "/api/assets/<int>/status").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto value = dto::require_string(body, "status", 32);
          const auto parsed = parse_asset_status(value);
          if (!parsed.has_value()) {
            throw invalid_request("status must be ACTIVE or CLOSED");
          }
          return dto::to_json(service_.update_status(id, *parsed));
        });
      });

  CROW_ROUTE(app, "/api/assets/<int>/detail").methods("PUT"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
          const auto body = dto::parse_object(request.body);
          const auto type_value = dto::require_string(body, "detail_type", 32);
          const auto type = parse_asset_type(type_value);
          if (!type.has_value()) {
            throw invalid_request("invalid detail_type");
          }
          const auto term_deposit = parse_term_deposit(body);
          const auto fund = parse_fund(body);
          const auto bond = parse_bond(body);
          const auto insurance = parse_insurance(body);
          return dto::to_json(service_.update_detail(
              id, *type, term_deposit ? &*term_deposit : nullptr,
              fund ? &*fund : nullptr, bond ? &*bond : nullptr,
              insurance ? &*insurance : nullptr));
        });
      });
}

}  // namespace wt

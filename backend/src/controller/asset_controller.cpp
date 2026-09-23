// 资产控制器实现：解析请求（含各类型明细块）-> 调用 AssetService -> 序列化 JSON。
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

// 解析可选日期字段：未提供返回 nullopt，提供了则校验 ISO8601 日期格式。
std::optional<std::string> parse_date_field(const nlohmann::json& object, const char* key,
                                            std::size_t max_length) {
  const auto value = dto::optional_string(object, key, max_length);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return time_util::require_date(*value, key);
}

// 解析定期存款明细块：字段整体缺省或为 null 时返回 nullopt（表示不提供该明细）；
// 存在则必须是对象，否则报参数错误。
std::optional<TermDepositDetail> parse_term_deposit(const nlohmann::json& body) {
  if (!body.contains("term_deposit") || body.at("term_deposit").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("term_deposit");
  if (!object.is_object()) {
    throw invalid_request("term_deposit must be an object");
  }
  TermDepositDetail detail;
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

// 解析基金明细块：缺省 / null 返回 nullopt；存在则必须是对象。
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
  detail.fund_type = dto::optional_string(object, "fund_type", 32);
  detail.lock_start_date = parse_date_field(object, "lock_start_date", 10);
  detail.lock_end_date = parse_date_field(object, "lock_end_date", 10);
  return detail;
}

// 解析债券明细块：缺省 / null 返回 nullopt；存在则必须是对象。
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
  detail.annual_coupon_rate =
      dto::optional_int64(object, "annual_coupon_rate").value_or(0);
  detail.purchase_date = parse_date_field(object, "purchase_date", 10);
  detail.maturity_date = parse_date_field(object, "maturity_date", 10);
  detail.lock_end_date = parse_date_field(object, "lock_end_date", 10);
  return detail;
}

// 解析债券基金明细块：缺省 / null 返回 nullopt；存在则必须是对象。
// purchase_date / holding_mode / holding_period_days 必填，其余选填；
// first_redeem_date / next_redeem_date 允许用户手工修正，缺省时由服务层计算。
std::optional<BondFundDetail> parse_bond_fund(const nlohmann::json& body) {
  if (!body.contains("bond_fund") || body.at("bond_fund").is_null()) {
    return std::nullopt;
  }
  const auto& object = body.at("bond_fund");
  if (!object.is_object()) {
    throw invalid_request("bond_fund must be an object");
  }
  BondFundDetail detail;
  detail.fund_code = dto::optional_string(object, "fund_code", 32);
  detail.expected_annual_yield_rate =
      dto::optional_int64(object, "expected_annual_yield_rate");
  detail.purchase_date =
      time_util::require_date(dto::require_string(object, "purchase_date", 10), "purchase_date");
  const auto mode = dto::require_string(object, "holding_mode", 16);
  const auto parsed_mode = parse_holding_mode(mode);
  if (!parsed_mode.has_value()) {
    throw invalid_request("holding_mode must be MIN_HOLDING or ROLLING");
  }
  detail.holding_mode = *parsed_mode;
  detail.holding_period_days = dto::require_int64(object, "holding_period_days");
  detail.first_redeem_date = parse_date_field(object, "first_redeem_date", 10);
  detail.next_redeem_date = parse_date_field(object, "next_redeem_date", 10);
  detail.maturity_date = parse_date_field(object, "maturity_date", 10);
  return detail;
}

// 解析保险明细块：缺省 / null 返回 nullopt；存在则必须是对象。
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

// 解析必填 asset_type：字段必填且取值必须是合法资产类型枚举。
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
  // GET /api/households/<int>/assets：列出资产聚合包，支持查询参数
  // owner_member_id 与 account_id 过滤。
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

  // POST /api/households/<int>/assets：新建资产；account_id/name/asset_type
  // 必填，opening_balance、remark 可选，四个明细块按资产类型选填。
  CROW_ROUTE(app, "/api/households/<int>/assets").methods("POST"_method)(
      [this](const crow::request& request, int id) {
        return http::handle([this, &request, id] {
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
          input.bond_fund = parse_bond_fund(body);
          input.insurance = parse_insurance(body);
          return dto::to_json(service_.create(id, input));
        });
      });

  // GET /api/assets/<int>：按 id 查询资产聚合包。
  CROW_ROUTE(app, "/api/assets/<int>").methods("GET"_method)([this](int id) {
    return http::handle([this, id] { return dto::to_json(service_.get_bundle(id)); });
  });

  // PUT /api/assets/<int>：更新资产元数据（名称、期初余额、备注）。
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

  // PUT /api/assets/<int>/status：更新资产状态（ACTIVE / CLOSED）。
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

  // PUT /api/assets/<int>/detail：按 detail_type 更新对应类型明细块；
  // 请求体中对应明细块缺省时以 nullptr 传入，语义由 Service 决定。
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
          const auto bond_fund = parse_bond_fund(body);
          const auto insurance = parse_insurance(body);
          return dto::to_json(service_.update_detail(
              id, *type, term_deposit ? &*term_deposit : nullptr,
              fund ? &*fund : nullptr, bond ? &*bond : nullptr,
              bond_fund ? &*bond_fund : nullptr,
              insurance ? &*insurance : nullptr));
        });
      });

  // DELETE /api/assets/<int>：删除资产；资产不存在抛 404，成功返回 null。
  // 注意：其名下全部流水与明细块会被级联删除，属不可恢复操作。
  CROW_ROUTE(app, "/api/assets/<int>").methods("DELETE"_method)([this](int id) {
    return http::handle([this, id] {
      service_.remove(id);
      return nlohmann::json(nullptr);
    });
  });
}

}  // namespace wt

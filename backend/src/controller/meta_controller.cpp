// 元数据控制器实现：返回枚举取值、默认收支分类及金额 / 利率单位约定。
#include "controller/meta_controller.hpp"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "controller/http_util.hpp"
#include "model/enums.hpp"
#include "utils/money.hpp"
#include "utils/rate.hpp"
#include "utils/time_util.hpp"

namespace wt {

void MetaController::register_routes(crow::SimpleApp& app) {
  // GET /api/meta：返回前端所需的全部枚举、默认分类与单位信息；
  // money.unit=minor 表示金额单位为分，rate_scale 为利率定点缩放因子。
  CROW_ROUTE(app, "/api/meta").methods("GET"_method)([this] {
    return http::handle([this] {
      nlohmann::json data;
      data["member_roles"] = {"OWNER", "MEMBER"};
      data["member_statuses"] = {"ACTIVE", "INACTIVE"};
      data["account_types"] = {"BANK",     "ALIPAY",   "WECHAT", "CASH",
                               "SECURITIES", "INSURANCE", "OTHER"};
      data["asset_types"] = {"CASH", "TERM_DEPOSIT", "STOCK_FUND",
                             "BOND_FUND", "FLEXIBLE_TERM", "COMMERCIAL_PENSION",
                             "INSURANCE", "LIABILITY", "OTHER"};
      data["asset_statuses"] = {"ACTIVE", "CLOSED"};
      data["transaction_types"] = {"INCOME", "EXPENSE", "TRANSFER_IN", "TRANSFER_OUT",
                                   "ADJUSTMENT", "ASSET_PURCHASE"};
      data["transaction_statuses"] = {"NORMAL", "VOID"};
      data["term_units"] = {"DAY", "MONTH", "YEAR"};
      data["income_categories"] = categories_.income;
      data["expense_categories"] = categories_.expense;
      data["money"] = {{"unit", "minor"}, {"minor_units_per_yuan", money::kMinorUnitsPerYuan}};
      data["rate_scale"] = rate::kScale;
      // 业务时区与业务日期：前端据此展示/判断日期，避免前后端各自用不同「今天」。
      data["business_timezone"] = time_util::business_timezone();
      data["business_date"] = time_util::business_today();
      return data;
    });
  });
}

}  // namespace wt

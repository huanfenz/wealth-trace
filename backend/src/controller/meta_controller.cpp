#include "controller/meta_controller.hpp"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "controller/http_util.hpp"
#include "model/enums.hpp"
#include "utils/money.hpp"
#include "utils/rate.hpp"

namespace wt {

void MetaController::register_routes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/meta").methods("GET"_method)([this] {
    return http::handle([this] {
      nlohmann::json data;
      data["member_roles"] = {"OWNER", "MEMBER"};
      data["member_statuses"] = {"ACTIVE", "INACTIVE"};
      data["account_types"] = {"BANK",     "ALIPAY",   "WECHAT", "CASH",
                               "SECURITIES", "INSURANCE", "OTHER"};
      data["asset_types"] = {"CASH",     "TERM_DEPOSIT", "FUND", "BOND",
                             "INSURANCE", "LIABILITY",    "OTHER"};
      data["asset_statuses"] = {"ACTIVE", "CLOSED"};
      data["transaction_types"] = {"INCOME", "EXPENSE", "TRANSFER_IN", "TRANSFER_OUT",
                                   "ADJUSTMENT"};
      data["transaction_statuses"] = {"NORMAL", "VOID"};
      data["term_units"] = {"DAY", "MONTH", "YEAR"};
      data["income_categories"] = categories_.income;
      data["expense_categories"] = categories_.expense;
      data["money"] = {{"unit", "minor"}, {"minor_units_per_yuan", money::kMinorUnitsPerYuan}};
      data["rate_scale"] = rate::kScale;
      return data;
    });
  });
}

}  // namespace wt

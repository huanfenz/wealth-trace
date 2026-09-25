#pragma once

#include <string>

#include "model/entities.hpp"
#include "utils/term_date.hpp"

namespace wt::commercial_pension {

inline std::string add_period(const std::string& time, const CommercialPensionDetail& detail) {
  return time_util::add_term(time.substr(0, 10), detail.holding_period_value,
                             detail.holding_period_unit) + time.substr(10);
}

// 返回当前尚可选择赎回的到期点；在到期的那一秒仍可选本期。
inline std::string selectable_maturity(const CommercialPensionDetail& detail,
                                       const std::string& now) {
  std::string maturity = add_period(detail.purchase_time, detail);
  while (maturity < now) maturity = add_period(maturity, detail);
  return maturity;
}

// 自动续期时，到期时刻立即进入下一期；赎回选择则锁定切换时的目标到期点。
inline std::string current_maturity(const CommercialPensionDetail& detail,
                                    const std::string& now) {
  if (detail.redeem_at_maturity && detail.redeem_at.has_value()) return *detail.redeem_at;
  std::string maturity = add_period(detail.purchase_time, detail);
  while (maturity <= now) maturity = add_period(maturity, detail);
  return maturity;
}

inline std::string reservation_status(const CommercialPensionDetail& detail,
                                      const std::string& now) {
  if (!detail.reservation_window_start.has_value() ||
      !detail.reservation_window_end.has_value()) return "NOT_SET";
  if (now < *detail.reservation_window_start) return "UPCOMING";
  if (now <= *detail.reservation_window_end) return "OPEN";
  return "ENDED";
}

}  // namespace wt::commercial_pension

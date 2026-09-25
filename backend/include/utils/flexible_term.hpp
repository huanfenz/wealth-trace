#pragma once

#include <string>

#include "model/entities.hpp"
#include "utils/term_date.hpp"
#include "utils/time_util.hpp"

namespace wt::flexible_term {

inline std::string maturity_date(const FlexibleTermDetail& detail) {
  return time_util::add_days(detail.purchase_date, detail.holding_period_days);
}

// 返回下一次可操作转出的日期；持有期满后今天即可转出。
inline std::string next_transfer_date(const FlexibleTermDetail& detail,
                                      const std::string& today) {
  const auto maturity = maturity_date(detail);
  if (today >= maturity) return today;
  const auto thirty_days = time_util::add_days(detail.purchase_date, 30);
  const auto earliest = today > thirty_days ? today : thirty_days;
  std::string fifth = earliest.substr(0, 7) + "-05";
  if (fifth < earliest) fifth = time_util::add_term(fifth, 1, TermUnit::Month);
  return fifth < maturity ? fifth : maturity;
}

inline bool can_transfer(const FlexibleTermDetail& detail, const std::string& today) {
  return next_transfer_date(detail, today) == today;
}

}  // namespace wt::flexible_term

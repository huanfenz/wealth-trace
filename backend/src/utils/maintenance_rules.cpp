// 资产时间状态维护规则实现：只做纯日期推进，不访问数据库。
#include "utils/maintenance_rules.hpp"

#include "utils/term_date.hpp"
#include "utils/time_util.hpp"

namespace wt::maintenance_rules {

std::optional<std::string> advance_bond_fund_next(std::string_view next,
                                                  std::int64_t holding_period_days,
                                                  std::string_view today) {
  if (holding_period_days <= 0) {
    return std::nullopt;
  }
  std::string advanced(next);
  bool changed = false;
  // 关键：必须用 today > next。today == next 代表今天仍是有效赎回日，
  // 若用 >= 会在赎回日 0 点直接滚入下一期，用户就看不到「今日可赎回」。
  while (today > advanced) {
    advanced = time_util::add_days(advanced, holding_period_days);
    changed = true;
  }
  if (!changed) {
    return std::nullopt;
  }
  return advanced;
}

std::optional<TermPeriod> advance_term_period(std::string_view maturity,
                                              std::int64_t term_value, TermUnit unit,
                                              std::string_view today) {
  if (term_value <= 0) {
    return std::nullopt;
  }
  std::string current(maturity);
  std::string start = current;
  bool changed = false;
  // 到期即续存：与滚动债基的 today > next 不同，自动续存用 today >= maturity，
  // 到期当天即进入下一存期。旧 maturity 作为新一期起始日，避免停机导致周期漂移。
  while (today >= current) {
    start = current;
    current = time_util::add_term(current, term_value, unit);
    changed = true;
  }
  if (!changed) {
    return std::nullopt;
  }
  return TermPeriod{start, current};
}

}  // namespace wt::maintenance_rules

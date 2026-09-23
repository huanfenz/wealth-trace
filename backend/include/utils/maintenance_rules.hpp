// 资产时间状态维护规则（纯函数）：把「与时间相关的日期」一次收敛到今天应有的状态。
// 每日维护、添加时维护与维护预览共用这里的推进规则，避免多处口径不一致。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "model/enums.hpp"

namespace wt::maintenance_rules {

// 滚动持有债券基金：从 next_redeem_date 出发按持有周期推进，直到 > today。
// 注意 today == next 当天仍是有效赎回日，故用 today > next 判断。
// 返回推进后的日期；无需推进（持有周期非正或日期非法除外）时返回 nullopt。
std::optional<std::string> advance_bond_fund_next(std::string_view next,
                                                  std::int64_t holding_period_days,
                                                  std::string_view today);

// 自动续存定期存款的当前存期区间。
struct TermPeriod {
  std::string start;     // 当前存期起始日
  std::string maturity;  // 当前存期到期日
};

// 自动续存定期存款：到期当天（today >= maturity）即进入下一期，直到 maturity > today。
// 返回推进后的 {start, maturity}；无需推进时返回 nullopt。
std::optional<TermPeriod> advance_term_period(std::string_view maturity,
                                              std::int64_t term_value, TermUnit unit,
                                              std::string_view today);

}  // namespace wt::maintenance_rules

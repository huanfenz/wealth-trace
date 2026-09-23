// 存期日期运算：按自然日 / 自然月 / 自然年推进日期，用于定期存款到期日与自动续存。
// 规则：目标月份不存在对应日期时落到该月最后一天（月末/闰年规则）。
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "model/enums.hpp"

namespace wt::time_util {

// 自然月加法："2026-01-31" + 1 月 -> "2026-02-28"；"2026-03-31" - 1 月 -> "2026-02-28"。
std::string add_months(std::string_view date, std::int64_t months);

// 自然年加法（等价于 add_months(years * 12)）："2024-02-29" + 1 年 -> "2025-02-28"。
std::string add_years(std::string_view date, std::int64_t years);

// 按存期单位推进日期：DAY -> add_days，MONTH -> add_months，YEAR -> add_years。
// value 须为正；date 非法时抛 invalid_request。
std::string add_term(std::string_view date, std::int64_t value, TermUnit unit);

}  // namespace wt::time_util

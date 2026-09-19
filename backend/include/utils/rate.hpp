#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wt::rate {

// Annual rates are stored as fixed point integers with six decimal places, as
// required by the design document (DECIMAL(10,6)): 0.018500 -> 18500.
inline constexpr std::int64_t kScale = 1000000;

// "1.85" (percent) -> 18500, i.e. 1.85% == 0.0185.
std::int64_t parse_percent(std::string_view text);

// "0.0185" (ratio) -> 18500.
std::int64_t parse_ratio(std::string_view text);

// 18500 -> "1.85" (percent, no trailing zeros beyond two decimals).
std::string format_percent(std::int64_t scaled_rate);

// 18500 -> "0.018500" (six decimals).
std::string format_ratio(std::int64_t scaled_rate);

}  // namespace wt::rate

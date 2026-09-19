// 利率工具：利率为定点整数，RATE_SCALE=1000000（即 1.85% 存为 18500）。
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wt::rate {

// 年利率以六位小数的定点整数存储，对应设计文档中的 DECIMAL(10,6)：
// 0.018500 -> 18500。
inline constexpr std::int64_t kScale = 1000000;

// 百分比字符串解析："1.85"（百分数）-> 18500，即 1.85% == 0.0185。
std::int64_t parse_percent(std::string_view text);

// 小数比率字符串解析："0.0185"（比率）-> 18500。
std::int64_t parse_ratio(std::string_view text);

// 格式化为百分比：18500 -> "1.85"（去掉多余的小数尾零，最多两位）。
std::string format_percent(std::int64_t scaled_rate);

// 格式化为小数比率：18500 -> "0.018500"（固定六位小数）。
std::string format_ratio(std::int64_t scaled_rate);

}  // namespace wt::rate

// 金额工具：内部统一以最小货币单位整数（人民币「分」）存储与传输。
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wt::money {

// 所有金额都以整数最小货币单位存储和传输。
// 人民币（CNY）：1 元 = 100 分。
inline constexpr std::int64_t kMinorUnitsPerYuan = 100;

// 分转元字符串：12345 -> "123.45"。
std::string format(std::int64_t minor_units);

// 元字符串转分："123.45" -> 12345；格式非法时抛出 ApiError(invalid request)。
// 最多接受两位小数。
std::int64_t parse_yuan(std::string_view text);

}  // namespace wt::money

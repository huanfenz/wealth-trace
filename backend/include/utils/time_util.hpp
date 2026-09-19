// 时间工具：全项目统一使用 UTC 字符串，日期时间 "YYYY-MM-DD HH:MM:SS"，日期 "YYYY-MM-DD"。
#pragma once

#include <string>
#include <string_view>

namespace wt::time_util {

// 整个项目只用一种时间表示：UTC，"YYYY-MM-DD HH:MM:SS"。
std::string now_iso8601();

// 当前 UTC 日期："YYYY-MM-DD"。
std::string today_iso8601();

// 校验格式是否为合法的日期时间/日期（只校验格式，不校验真实历法范围）。
bool is_valid_datetime(std::string_view text);
bool is_valid_date(std::string_view text);

// 合法时原样返回；否则抛出 ApiError(invalid request)。
std::string require_datetime(std::string_view text, std::string_view field);
std::string require_date(std::string_view text, std::string_view field);

}  // namespace wt::time_util

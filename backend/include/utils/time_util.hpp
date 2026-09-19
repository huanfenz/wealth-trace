#pragma once

#include <string>
#include <string_view>

namespace wt::time_util {

// The whole project uses one time representation: UTC, "YYYY-MM-DD HH:MM:SS".
std::string now_iso8601();

// "YYYY-MM-DD" (UTC).
std::string today_iso8601();

bool is_valid_datetime(std::string_view text);
bool is_valid_date(std::string_view text);

// Returns text unchanged when valid, otherwise throws ApiError(invalid request).
std::string require_datetime(std::string_view text, std::string_view field);
std::string require_date(std::string_view text, std::string_view field);

}  // namespace wt::time_util

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wt::money {

// All monetary values are stored and transported as integer minor units.
// For CNY: 1 yuan = 100 fen.
inline constexpr std::int64_t kMinorUnitsPerYuan = 100;

// 12345 -> "123.45"
std::string format(std::int64_t minor_units);

// "123.45" -> 12345. Throws ApiError(invalid request) on malformed input.
// Up to two decimal places are accepted.
std::int64_t parse_yuan(std::string_view text);

}  // namespace wt::money

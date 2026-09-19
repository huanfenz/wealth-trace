#include "utils/rate.hpp"

#include <iomanip>
#include <sstream>
#include <string>

#include "common/error.hpp"

namespace wt::rate {
namespace {

std::string_view trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

// Parses a decimal number into `scale` fractional digits, padding with zeros and
// rejecting anything that would lose precision.
std::int64_t parse_scaled(std::string_view text, std::int64_t scale, int max_digits) {
  text = trim(text);
  if (text.empty()) {
    throw invalid_request("rate is required");
  }
  std::size_t index = 0;
  bool negative = false;
  if (text[index] == '+' || text[index] == '-') {
    negative = text[index] == '-';
    ++index;
  }
  std::int64_t integer_part = 0;
  bool has_digit = false;
  for (; index < text.size() && text[index] >= '0' && text[index] <= '9'; ++index) {
    has_digit = true;
    integer_part = integer_part * 10 + (text[index] - '0');
  }
  std::int64_t fraction = 0;
  int digits = 0;
  if (index < text.size() && text[index] == '.') {
    ++index;
    for (; index < text.size() && text[index] >= '0' && text[index] <= '9'; ++index) {
      if (digits >= max_digits) {
        throw invalid_request(std::string("rate has too many decimal places: ") +
                              std::string(text));
      }
      fraction = fraction * 10 + (text[index] - '0');
      ++digits;
    }
  }
  if (!has_digit || index != text.size()) {
    throw invalid_request(std::string("invalid rate: ") + std::string(text));
  }
  while (digits < max_digits) {
    fraction *= 10;
    ++digits;
  }
  const auto total = (integer_part * scale) + fraction;
  return negative ? -total : total;
}

}  // namespace

std::int64_t parse_percent(std::string_view text) {
  // percent -> ratio: divide by 100, i.e. scale the parsed number accordingly.
  const auto scaled = parse_scaled(text, kScale, 6);
  return scaled / 100;
}

std::int64_t parse_ratio(std::string_view text) {
  return parse_scaled(text, kScale, 6);
}

std::string format_percent(std::int64_t scaled_rate) {
  // scaled_rate is ratio * 1e6. Percent * 1e4 therefore equals scaled_rate
  // (ratio * 1e6 * 100 / 1e4 == ratio * 1e6). So the stored integer is already
  // the percent value scaled by 10000.
  const bool negative = scaled_rate < 0;
  const auto absolute = negative ? -scaled_rate : scaled_rate;
  const auto integer_part = absolute / 10000;
  const auto fraction = absolute % 10000;
  std::ostringstream out;
  if (negative) {
    out << '-';
  }
  out << integer_part;
  if (fraction != 0) {
    out << '.' << std::setw(4) << std::setfill('0') << fraction;
    std::string value = out.str();
    while (!value.empty() && value.back() == '0') {
      value.pop_back();
    }
    if (!value.empty() && value.back() == '.') {
      value.pop_back();
    }
    return value;
  }
  return out.str();
}

std::string format_ratio(std::int64_t scaled_rate) {
  const bool negative = scaled_rate < 0;
  const auto absolute = negative ? -scaled_rate : scaled_rate;
  std::ostringstream out;
  if (negative) {
    out << '-';
  }
  out << (absolute / kScale) << '.' << std::setw(6) << std::setfill('0')
      << (absolute % kScale);
  return out.str();
}

}  // namespace wt::rate

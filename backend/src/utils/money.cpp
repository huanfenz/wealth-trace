#include "utils/money.hpp"

#include <cstdlib>
#include <sstream>
#include <string>

#include "common/error.hpp"

namespace wt::money {
namespace {

std::string_view trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

}  // namespace

std::string format(std::int64_t minor_units) {
  const bool negative = minor_units < 0;
  const auto absolute = negative ? -minor_units : minor_units;
  std::ostringstream out;
  if (negative) {
    out << '-';
  }
  out << (absolute / kMinorUnitsPerYuan) << '.';
  const auto fraction = absolute % kMinorUnitsPerYuan;
  out.fill('0');
  out.width(2);
  out << fraction;
  return out.str();
}

std::int64_t parse_yuan(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    throw invalid_request("amount is required");
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
  int fraction_digits = 0;
  if (index < text.size() && text[index] == '.') {
    ++index;
    for (; index < text.size() && text[index] >= '0' && text[index] <= '9'; ++index) {
      if (fraction_digits >= 2) {
        throw invalid_request(std::string("amount has too many decimal places: ") +
                              std::string(text));
      }
      fraction = fraction * 10 + (text[index] - '0');
      ++fraction_digits;
    }
  }
  if (!has_digit || index != text.size()) {
    throw invalid_request(std::string("invalid amount: ") + std::string(text));
  }
  while (fraction_digits < 2) {
    fraction *= 10;
    ++fraction_digits;
  }
  const auto total = integer_part * kMinorUnitsPerYuan + fraction;
  return negative ? -total : total;
}

}  // namespace wt::money

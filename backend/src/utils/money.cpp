// 金额工具实现：分与元字符串之间的定点换算。
#include "utils/money.hpp"

#include <cstdlib>
#include <sstream>
#include <string>

#include "common/error.hpp"

namespace wt::money {
namespace {

// 去除首尾空白，返回原串视图；全空白时返回空视图。
std::string_view trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

}  // namespace

// 分转 "元.分" 字符串；整数部分与两位小数分别输出，负数保留符号。
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
  out.width(2);  // 不足两位的分补零，如 5 分 -> ".05"
  out << fraction;
  return out.str();
}

// 解析 "123.45" -> 12345 分；逐字符解析以避免浮点误差。
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
      // 金额最小单位为分，超过两位小数无法精确表示，直接拒绝。
      if (fraction_digits >= 2) {
        throw invalid_request(std::string("amount has too many decimal places: ") +
                              std::string(text));
      }
      fraction = fraction * 10 + (text[index] - '0');
      ++fraction_digits;
    }
  }
  // 无整数位、或存在未消费的非法字符（如字母、多个小数点）均视为非法。
  if (!has_digit || index != text.size()) {
    throw invalid_request(std::string("invalid amount: ") + std::string(text));
  }
  // 小数位不足两位时右移补零："1.5" -> 50 分。
  while (fraction_digits < 2) {
    fraction *= 10;
    ++fraction_digits;
  }
  const auto total = integer_part * kMinorUnitsPerYuan + fraction;
  return negative ? -total : total;
}

}  // namespace wt::money

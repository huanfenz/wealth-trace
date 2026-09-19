// 利率工具实现：百分比/比率字符串与定点整数之间的换算。
#include "utils/rate.hpp"

#include <iomanip>
#include <sstream>
#include <string>

#include "common/error.hpp"

namespace wt::rate {
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

// 将小数字符串解析为保留 scale 位小数的定点整数：
// 不足位数补零，超过 max_digits 位（会丢精度）则拒绝。
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
      // 超过最大精度位数的输入会引入舍入误差，直接拒绝。
      if (digits >= max_digits) {
        throw invalid_request(std::string("rate has too many decimal places: ") +
                              std::string(text));
      }
      fraction = fraction * 10 + (text[index] - '0');
      ++digits;
    }
  }
  // 无整数位、或存在未消费的非法字符均视为非法。
  if (!has_digit || index != text.size()) {
    throw invalid_request(std::string("invalid rate: ") + std::string(text));
  }
  // 小数位不足时右移补零，使其对齐到 max_digits 位。
  while (digits < max_digits) {
    fraction *= 10;
    ++digits;
  }
  const auto total = (integer_part * scale) + fraction;
  return negative ? -total : total;
}

}  // namespace

std::int64_t parse_percent(std::string_view text) {
  // 百分数 -> 比率：除以 100，即把按比例解析的结果再缩小 100 倍。
  const auto scaled = parse_scaled(text, kScale, 6);
  return scaled / 100;
}

std::int64_t parse_ratio(std::string_view text) {
  return parse_scaled(text, kScale, 6);
}

// 定点整数格式化为百分数字符串，并去除多余的尾零。
std::string format_percent(std::int64_t scaled_rate) {
  // scaled_rate = 比率 * 1e6；百分数 * 1e4 也等于该值
  // (比率 * 1e6 * 100 / 1e4 == 比率 * 1e6)。因此该整数本身就是
  // 放大 10000 倍后的百分数，直接按 4 位小数拆分。
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
    // 去掉尾部多余的 '0'，再去掉可能残留的小数点，如 "1.8500" -> "1.85"。
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

// 定点整数格式化为固定六位小数的比率字符串。
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

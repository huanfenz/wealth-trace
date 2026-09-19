// 时间工具实现：生成与校验 UTC 日期时间字符串。
#include "utils/time_util.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "common/error.hpp"

namespace wt::time_util {
namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

// 校验 "YYYY-MM-DD HH:MM:SS"：先卡总长度，再逐位检查分隔符与数字。
// 注意这里只校验格式，不校验月份/日期是否为真实历法日期。
bool is_valid_datetime_impl(std::string_view text) {
  // 期望格式：YYYY-MM-DD HH:MM:SS
  if (text.size() != 19) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
    // 位置 4、7 为 '-'，位置 10 为空格，位置 13、16 为 ':'，其余须为数字。
    const char expected_separator =
        (i == 4 || i == 7) ? '-' : (i == 10 ? ' ' : ((i == 13 || i == 16) ? ':' : '\0'));
    if (expected_separator != '\0') {
      if (text[i] != expected_separator) {
        return false;
      }
    } else if (!is_digit(text[i])) {
      return false;
    }
  }
  return true;
}

// 校验 "YYYY-MM-DD"：长度固定为 10，位置 4、7 为 '-'，其余须为数字。
bool is_valid_date_impl(std::string_view text) {
  if (text.size() != 10) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i == 4 || i == 7) {
      if (text[i] != '-') {
        return false;
      }
    } else if (!is_digit(text[i])) {
      return false;
    }
  }
  return true;
}

// 将时间戳按给定格式转为 UTC 字符串；gmtime 保证输出为 UTC 而非本地时区。
std::string format_time(std::time_t value, const char* pattern) {
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &value);
#else
  gmtime_r(&value, &tm);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), pattern, &tm);
  return buffer;
}

}  // namespace

std::string now_iso8601() {
  return format_time(std::time(nullptr), "%Y-%m-%d %H:%M:%S");
}

std::string today_iso8601() {
  return format_time(std::time(nullptr), "%Y-%m-%d");
}

bool is_valid_datetime(std::string_view text) {
  return is_valid_datetime_impl(text);
}

bool is_valid_date(std::string_view text) {
  return is_valid_date_impl(text);
}

// 校验失败时抛出携带字段名的参数错误。
std::string require_datetime(std::string_view text, std::string_view field) {
  if (!is_valid_datetime_impl(text)) {
    throw invalid_request(std::string(field) + " must be formatted as YYYY-MM-DD HH:MM:SS");
  }
  return std::string(text);
}

std::string require_date(std::string_view text, std::string_view field) {
  if (!is_valid_date_impl(text)) {
    throw invalid_request(std::string(field) + " must be formatted as YYYY-MM-DD");
  }
  return std::string(text);
}

}  // namespace wt::time_util

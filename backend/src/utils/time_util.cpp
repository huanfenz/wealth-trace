#include "utils/time_util.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "common/error.hpp"

namespace wt::time_util {
namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_valid_datetime_impl(std::string_view text) {
  // Expected: YYYY-MM-DD HH:MM:SS
  if (text.size() != 19) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
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

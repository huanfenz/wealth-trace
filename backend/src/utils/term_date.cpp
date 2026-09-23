// 存期日期运算实现：自然月/自然年推进 + 月末与闰年钳制。
#include "utils/term_date.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "common/error.hpp"
#include "utils/time_util.hpp"

namespace wt::time_util {
namespace {

bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int days_in_month(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return kDays[month - 1];
}

// 从 "YYYY-MM-DD" 取年/月/日；调用方需保证格式已校验。
void split(std::string_view date, int& year, int& month, int& day) {
  year = std::stoi(std::string(date.substr(0, 4)));
  month = std::stoi(std::string(date.substr(5, 2)));
  day = std::stoi(std::string(date.substr(8, 2)));
}

std::string format(int year, int month, int day) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
  return buffer;
}

}  // namespace

std::string add_months(std::string_view date, std::int64_t months) {
  const std::string valid = require_date(date, "date");
  int year = 0;
  int month = 0;
  int day = 0;
  split(valid, year, month, day);

  // 先归一到 0 基月份再平移，避免负数取模的分支问题。
  std::int64_t total = static_cast<std::int64_t>(year) * 12 + (month - 1) + months;
  std::int64_t new_year = total / 12;
  std::int64_t new_month0 = total % 12;
  if (new_month0 < 0) {
    new_month0 += 12;
    --new_year;
  }
  const int target_year = static_cast<int>(new_year);
  const int target_month = static_cast<int>(new_month0) + 1;
  // 月末/闰年规则：目标月没有这一天时取该月最后一天。
  const int target_day = std::min(day, days_in_month(target_year, target_month));
  return format(target_year, target_month, target_day);
}

std::string add_years(std::string_view date, std::int64_t years) {
  return add_months(date, years * 12);
}

std::string add_term(std::string_view date, std::int64_t value, TermUnit unit) {
  if (value <= 0) {
    throw invalid_request("term value must be positive");
  }
  switch (unit) {
    case TermUnit::Day:
      return add_days(date, value);
    case TermUnit::Month:
      return add_months(date, value);
    case TermUnit::Year:
      return add_years(date, value);
  }
  throw invalid_request("unsupported term unit");
}

}  // namespace wt::time_util

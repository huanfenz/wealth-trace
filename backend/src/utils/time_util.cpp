// 时间工具实现：生成与校验 UTC 日期时间字符串。
#include "utils/time_util.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>

#include "common/error.hpp"

namespace wt::time_util {
namespace {

// 业务时区：默认 Asia/Shanghai。通过 setenv("TZ") + tzset 让 localtime_r 按该时区解析。
// 只在启动阶段（创建线程前）调用 set_business_timezone，之后仅做只读的 localtime_r，
// 因此对多线程读取是安全的。Asia/Shanghai 无夏令时，跨时区规则由系统 tzdata 提供。
std::mutex& tz_mutex() {
  static std::mutex mutex;
  return mutex;
}
std::string& tz_name() {
  static std::string name = "Asia/Shanghai";
  return name;
}
bool& tz_applied() {
  static bool applied = false;
  return applied;
}

// 应用 TZ 环境变量（持有 tz_mutex 时调用）。
void apply_timezone_locked() {
  if (tz_applied()) {
    return;
  }
#if defined(_WIN32)
  _putenv_s("TZ", tz_name().c_str());
  _tzset();
#else
  setenv("TZ", tz_name().c_str(), 1);
  tzset();
#endif
  tz_applied() = true;
}

// 把时间戳转换为业务时区的本地时间（线程安全的 localtime_r）。
std::tm business_localtime(std::time_t value) {
  std::scoped_lock lock(tz_mutex());
  apply_timezone_locked();
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &value);
#else
  localtime_r(&value, &tm);
#endif
  return tm;
}

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

// 公历日期 <-> 自 1970-01-01 的天数（Howard Hinnant 的 civil 算法）。
// 手动实现而不依赖 std::chrono 日历，避免不同编译器对 C++20 日历支持不一致。
int days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy =
      (153 * (month + (month > 2 ? -3u : 9u)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

void civil_from_days(int days, int& year, unsigned& month, unsigned& day) {
  days += 719468;
  const int era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int y = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp + (mp < 10 ? 3u : -9u);
  year = y + (month <= 2 ? 1 : 0);
}

// 从 "YYYY-MM-DD" 中取出年/月/日（调用方需保证格式合法）。
void split_date(std::string_view date, int& year, unsigned& month, unsigned& day) {
  year = std::stoi(std::string(date.substr(0, 4)));
  month = static_cast<unsigned>(std::stoi(std::string(date.substr(5, 2))));
  day = static_cast<unsigned>(std::stoi(std::string(date.substr(8, 2))));
}

// 从 "YYYY-MM-DD HH:MM:SS" 解析出各字段（调用方需保证格式合法）。
void split_datetime(std::string_view text, int& year, unsigned& month, unsigned& day,
                    int& hour, int& minute, int& second) {
  split_date(text.substr(0, 10), year, month, day);
  hour = std::stoi(std::string(text.substr(11, 2)));
  minute = std::stoi(std::string(text.substr(14, 2)));
  second = std::stoi(std::string(text.substr(17, 2)));
}

// 把 UTC 时间戳格式化为 "YYYY-MM-DD HH:MM:SS"。
std::string format_utc(std::time_t value) {
  return format_time(value, "%Y-%m-%d %H:%M:%S");
}

}  // namespace

std::string now_iso8601() {
  return format_time(std::time(nullptr), "%Y-%m-%d %H:%M:%S");
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

std::string add_days(std::string_view date, std::int64_t days) {
  if (!is_valid_date_impl(date)) {
    throw invalid_request("date must be formatted as YYYY-MM-DD");
  }
  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  split_date(date, year, month, day);
  const std::int64_t shifted =
      static_cast<std::int64_t>(days_from_civil(year, month, day)) + days;
  int out_year = 0;
  unsigned out_month = 0;
  unsigned out_day = 0;
  civil_from_days(static_cast<int>(shifted), out_year, out_month, out_day);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", out_year, out_month, out_day);
  return buffer;
}

std::int64_t days_between(std::string_view from, std::string_view to) {
  if (!is_valid_date_impl(from) || !is_valid_date_impl(to)) {
    throw invalid_request("date must be formatted as YYYY-MM-DD");
  }
  int from_year = 0;
  int to_year = 0;
  unsigned from_month = 0;
  unsigned to_month = 0;
  unsigned from_day = 0;
  unsigned to_day = 0;
  split_date(from, from_year, from_month, from_day);
  split_date(to, to_year, to_month, to_day);
  return static_cast<std::int64_t>(days_from_civil(to_year, to_month, to_day)) -
         static_cast<std::int64_t>(days_from_civil(from_year, from_month, from_day));
}

void set_business_timezone(std::string_view tz) {
  std::scoped_lock lock(tz_mutex());
  if (!tz.empty()) {
    tz_name() = std::string(tz);
  }
  tz_applied() = false;
  apply_timezone_locked();
}

std::string business_timezone() {
  std::scoped_lock lock(tz_mutex());
  return tz_name();
}

std::string business_today() {
  const std::tm tm = business_localtime(std::time(nullptr));
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday);
  return buffer;
}

std::int64_t seconds_until_next_business_midnight() {
  const std::tm tm = business_localtime(std::time(nullptr));
  const std::int64_t elapsed =
      static_cast<std::int64_t>(tm.tm_hour) * 3600 + tm.tm_min * 60 + tm.tm_sec;
  std::int64_t remaining = 86400 - elapsed;
  if (remaining <= 0) {
    remaining = 86400;
  }
  return remaining;
}

std::string business_to_utc(std::string_view local_datetime) {
  if (!is_valid_datetime_impl(local_datetime)) {
    throw invalid_request("datetime must be formatted as YYYY-MM-DD HH:MM:SS");
  }
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  unsigned month = 0;
  unsigned day = 0;
  split_datetime(local_datetime, year, month, day, hour, minute, second);
  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = static_cast<int>(month) - 1;
  tm.tm_mday = static_cast<int>(day);
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = -1;
  std::time_t epoch = 0;
  {
    // mktime 依赖全局 TZ，故与其它时区读写共用一把锁；TZ 只写一次，成本可忽略。
    std::scoped_lock lock(tz_mutex());
    apply_timezone_locked();
    epoch = std::mktime(&tm);
  }
  if (epoch == static_cast<std::time_t>(-1)) {
    throw invalid_request("datetime out of range");
  }
  return format_utc(epoch);
}

std::string utc_to_business(std::string_view utc_datetime) {
  if (!is_valid_datetime_impl(utc_datetime)) {
    throw invalid_request("datetime must be formatted as YYYY-MM-DD HH:MM:SS");
  }
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  unsigned month = 0;
  unsigned day = 0;
  split_datetime(utc_datetime, year, month, day, hour, minute, second);
  const std::int64_t days = days_from_civil(year, month, day);
  const std::time_t epoch = static_cast<std::time_t>(days * 86400 + hour * 3600 +
                                                     minute * 60 + second);
  const std::tm local = business_localtime(epoch);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                local.tm_min, local.tm_sec);
  return buffer;
}

}  // namespace wt::time_util

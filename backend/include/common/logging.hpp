#pragma once

#include <string>
#include <string_view>

namespace wt {

enum class LogLevel : int {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Critical = 5,
};

LogLevel parse_log_level(std::string_view text);
std::string_view to_string(LogLevel level);

void set_log_level(LogLevel level);
LogLevel log_level();

void log_message(LogLevel level, std::string_view message);

void log_trace(std::string_view message);
void log_debug(std::string_view message);
void log_info(std::string_view message);
void log_warn(std::string_view message);
void log_error(std::string_view message);
void log_critical(std::string_view message);

}  // namespace wt

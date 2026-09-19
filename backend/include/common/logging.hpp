// 日志模块接口：定义日志级别、级别解析/格式化以及各级别便捷输出函数。
#pragma once

#include <string>
#include <string_view>

namespace wt {

// 日志级别，数值越大越严重；比较时按数值大小判断是否输出。
enum class LogLevel : int {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Critical = 5,
};

// 解析配置中的级别文本（如 "info"、"warning"）；无法识别时回退为 Info。
LogLevel parse_log_level(std::string_view text);
// 级别转小写文本（如 "info"），用于配置回显。
std::string_view to_string(LogLevel level);

// 设置全局最低输出级别（线程安全）。
void set_log_level(LogLevel level);
// 读取当前全局最低输出级别。
LogLevel log_level();

// 按级别输出一条日志；低于当前全局级别时直接丢弃。
void log_message(LogLevel level, std::string_view message);

// 下列为各日志级别的便捷封装。
void log_trace(std::string_view message);
void log_debug(std::string_view message);
void log_info(std::string_view message);
void log_warn(std::string_view message);
void log_error(std::string_view message);
void log_critical(std::string_view message);

}  // namespace wt

// 日志模块实现：UTC 时间戳、线程安全输出与按级别过滤。
#include "common/logging.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace wt {
namespace {

// 全局最低输出级别；用原子量保证多线程读写无数据竞争，默认 Info。
std::atomic<int> g_log_level{static_cast<int>(LogLevel::Info)};
// 串行化输出，避免多线程日志互相穿插。
std::mutex g_log_mutex;

// 级别到输出标签（大写）的映射。
const char* level_tag(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Critical: return "CRITICAL";
  }
  return "INFO";
}

// 生成 UTC 时间戳，格式为 "YYYY-MM-DD HH:MM:SS.mmm"（毫秒补零到 3 位）。
std::string utc_timestamp() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto seconds = system_clock::to_time_t(now);
  // 取毫秒部分：注意先对 1000 取模，避免用秒级精度丢失毫秒。
  const auto millis = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &seconds);
#else
  gmtime_r(&seconds, &tm);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
  std::ostringstream out;
  out << buffer;
  out.fill('0');
  out.width(3);
  out << millis.count();
  return out.str();
}

}  // namespace

// 解析级别文本，大小写敏感；未知值回退为 Info，保证配置容错。
LogLevel parse_log_level(std::string_view text) {
  if (text == "trace") return LogLevel::Trace;
  if (text == "debug") return LogLevel::Debug;
  if (text == "info") return LogLevel::Info;
  if (text == "warn" || text == "warning") return LogLevel::Warn;
  if (text == "error") return LogLevel::Error;
  if (text == "critical") return LogLevel::Critical;
  return LogLevel::Info;
}

// 级别转小写文本，供配置回显使用。
std::string_view to_string(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    case LogLevel::Critical: return "critical";
  }
  return "info";
}

void set_log_level(LogLevel level) {
  // relaxed 足够：日志级别只需最终可见，不依赖与其他内存操作的顺序。
  g_log_level.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogLevel log_level() {
  return static_cast<LogLevel>(g_log_level.load(std::memory_order_relaxed));
}

void log_message(LogLevel level, std::string_view message) {
  // 低于全局级别的日志直接丢弃，避免不必要的加锁与格式化开销。
  if (static_cast<int>(level) < g_log_level.load(std::memory_order_relaxed)) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::cerr << utc_timestamp() << " [" << level_tag(level) << "] " << message << '\n';
}

void log_trace(std::string_view message) { log_message(LogLevel::Trace, message); }
void log_debug(std::string_view message) { log_message(LogLevel::Debug, message); }
void log_info(std::string_view message) { log_message(LogLevel::Info, message); }
void log_warn(std::string_view message) { log_message(LogLevel::Warn, message); }
void log_error(std::string_view message) { log_message(LogLevel::Error, message); }
void log_critical(std::string_view message) { log_message(LogLevel::Critical, message); }

}  // namespace wt

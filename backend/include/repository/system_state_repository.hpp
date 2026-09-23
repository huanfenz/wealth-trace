// 系统键值元数据（system_state）数据访问。用于记录每日维护最近一次成功执行的日期等。
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace wt {

class Database;

// system_state 表读写：简单 key -> value 映射，value 以文本存储。
class SystemStateRepository {
 public:
  explicit SystemStateRepository(Database& database) : database_(database) {}

  // 读取键值，不存在返回 std::nullopt。
  std::optional<std::string> get(std::string_view key);
  // 写入或覆盖键值（INSERT ... ON CONFLICT DO UPDATE）。
  void set(std::string_view key, std::string_view value);

 private:
  Database& database_;
};

}  // namespace wt

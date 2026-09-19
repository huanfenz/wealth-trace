// 字符串工具实现。
#include "utils/strings.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "common/error.hpp"

namespace wt::strings {

// 去除首尾空白，返回原串视图；全空白时返回空视图。
std::string_view trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

// 逐字节转大写；先转 unsigned char 再调用 toupper，避免负值字符的未定义行为。
std::string to_upper(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return result;
}

bool is_blank(std::string_view text) { return trim(text).empty(); }

// 必填字段校验：去空白后为空或超长均报错，返回去空白后的副本。
std::string require_text(std::string_view value, std::string_view field,
                         std::size_t max_length) {
  const auto trimmed = trim(value);
  if (trimmed.empty()) {
    throw invalid_request(std::string(field) + " is required");
  }
  if (trimmed.size() > max_length) {
    throw invalid_request(std::string(field) + " is too long (max " +
                          std::to_string(max_length) + " characters)");
  }
  return std::string(trimmed);
}

// 可空字段：允许为空，仅做长度上限校验，返回去空白后的副本。
std::string optional_text(std::string_view value, std::string_view field,
                          std::size_t max_length) {
  const auto trimmed = trim(value);
  if (trimmed.size() > max_length) {
    throw invalid_request(std::string(field) + " is too long (max " +
                          std::to_string(max_length) + " characters)");
  }
  return std::string(trimmed);
}

}  // namespace wt::strings

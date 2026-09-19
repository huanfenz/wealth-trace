// 字符串工具：空白处理、大小写转换与带字段名的文本校验。
#pragma once

#include <string>
#include <string_view>

namespace wt::strings {

// 去除首尾的空白字符（空格、制表符、回车、换行），返回原串的视图。
std::string_view trim(std::string_view text);
// 转为大写，返回新字符串。
std::string to_upper(std::string_view text);
// 是否为空或仅含空白。
bool is_blank(std::string_view text);

// 返回去除首尾空白后的副本；为空或超过 max_length 时
// 抛出 ApiError(invalid request)。
std::string require_text(std::string_view value, std::string_view field,
                         std::size_t max_length);
// 可空字段：返回去除首尾空白后的副本，仅在超过 max_length 时抛错。
std::string optional_text(std::string_view value, std::string_view field,
                          std::size_t max_length);

}  // namespace wt::strings

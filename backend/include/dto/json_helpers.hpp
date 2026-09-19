#pragma once

// DTO 请求体解析辅助：把 HTTP 请求里的 JSON 对象按字段取出并做基础校验。
// 校验失败统一抛 invalid_request（映射为 40001/400），由控制器统一错误处理。
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace wt::dto {

// 解析 JSON 请求体并确保顶层是对象。空体、非法 JSON 或非对象都会抛
// invalid_request。返回的对象后续各字段读取函数直接复用。
nlohmann::json parse_object(std::string_view body);

// 必填字符串：缺省或 null 视为未提供并报错；类型非字符串报错；
// 再经 strings::require_text 做去空白与最大长度校验。返回去空白后的值。
std::string require_string(const nlohmann::json& object, const char* key,
                           std::size_t max_length);
// 可选字符串：缺省或 null 均视为「未提供」，返回 nullopt；若提供了但类型
// 不是字符串则报错；返回值经去空白与长度校验（空串会返回空字符串）。
std::optional<std::string> optional_string(const nlohmann::json& object, const char* key,
                                           std::size_t max_length);
// 可选字符串带默认值：未提供或提供空串时返回 fallback。
std::string optional_string_or(const nlohmann::json& object, const char* key,
                               const std::string& fallback);
// 必填 64 位整数：缺省或 null 视为未提供并报错；类型非整数（含浮点）报错。
std::int64_t require_int64(const nlohmann::json& object, const char* key);
// 可选 64 位整数：缺省或 null 均视为「未提供」返回 nullopt；类型非整数报错。
std::optional<std::int64_t> optional_int64(const nlohmann::json& object, const char* key);
// 可选布尔：缺省或 null 一律返回 fallback；类型非布尔报错。
bool optional_bool(const nlohmann::json& object, const char* key, bool fallback);

}  // namespace wt::dto

// DTO 请求体解析辅助实现：统一「缺省 / null 视为未提供」的语义与
// 类型校验，失败时抛 invalid_request 交给控制器转换为 40001 响应。
#include "dto/json_helpers.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "utils/strings.hpp"

namespace wt::dto {

// 解析请求体：空白体报「必填」，parse(..., false) 为不抛异常模式，
// 解析失败得到 discarded 值再统一转成 invalid_request。
nlohmann::json parse_object(std::string_view body) {
  if (strings::is_blank(body)) {
    throw invalid_request("request body is required");
  }
  nlohmann::json object = nlohmann::json::parse(body, nullptr, false);
  if (object.is_discarded()) {
    throw invalid_request("request body is not valid JSON");
  }
  if (!object.is_object()) {
    throw invalid_request("request body must be a JSON object");
  }
  return object;
}

// 必填字符串。
std::string require_string(const nlohmann::json& object, const char* key,
                           std::size_t max_length) {
  if (!object.contains(key) || object.at(key).is_null()) {
    throw invalid_request(std::string(key) + " is required");
  }
  if (!object.at(key).is_string()) {
    throw invalid_request(std::string(key) + " must be a string");
  }
  return strings::require_text(object.at(key).get<std::string>(), key, max_length);
}

// 可选字符串：缺省 / null 返回 nullopt；提供字符串则去空白并校验长度（空串返回空串）。
std::optional<std::string> optional_string(const nlohmann::json& object, const char* key,
                                           std::size_t max_length) {
  if (!object.contains(key) || object.at(key).is_null()) {
    return std::nullopt;
  }
  if (!object.at(key).is_string()) {
    throw invalid_request(std::string(key) + " must be a string");
  }
  return strings::optional_text(object.at(key).get<std::string>(), key, max_length);
}

// 可选字符串带默认值（公共场景按 1000 长度上限校验）。
std::string optional_string_or(const nlohmann::json& object, const char* key,
                               const std::string& fallback) {
  const auto value = optional_string(object, key, 1000);
  if (!value.has_value() || value->empty()) {
    return fallback;
  }
  return *value;
}

// 必填 64 位整数。
std::int64_t require_int64(const nlohmann::json& object, const char* key) {
  if (!object.contains(key) || object.at(key).is_null()) {
    throw invalid_request(std::string(key) + " is required");
  }
  if (!object.at(key).is_number_integer()) {
    throw invalid_request(std::string(key) + " must be an integer");
  }
  return object.at(key).get<std::int64_t>();
}

// 可选 64 位整数。
std::optional<std::int64_t> optional_int64(const nlohmann::json& object,
                                           const char* key) {
  if (!object.contains(key) || object.at(key).is_null()) {
    return std::nullopt;
  }
  if (!object.at(key).is_number_integer()) {
    throw invalid_request(std::string(key) + " must be an integer");
  }
  return object.at(key).get<std::int64_t>();
}

// 可选布尔，缺省 / null 返回 fallback。
bool optional_bool(const nlohmann::json& object, const char* key, bool fallback) {
  if (!object.contains(key) || object.at(key).is_null()) {
    return fallback;
  }
  if (!object.at(key).is_boolean()) {
    throw invalid_request(std::string(key) + " must be a boolean");
  }
  return object.at(key).get<bool>();
}

}  // namespace wt::dto

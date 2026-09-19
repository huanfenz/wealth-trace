#include "dto/json_helpers.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/error.hpp"
#include "utils/strings.hpp"

namespace wt::dto {

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

std::string optional_string_or(const nlohmann::json& object, const char* key,
                               const std::string& fallback) {
  const auto value = optional_string(object, key, 1000);
  if (!value.has_value() || value->empty()) {
    return fallback;
  }
  return *value;
}

std::int64_t require_int64(const nlohmann::json& object, const char* key) {
  if (!object.contains(key) || object.at(key).is_null()) {
    throw invalid_request(std::string(key) + " is required");
  }
  if (!object.at(key).is_number_integer()) {
    throw invalid_request(std::string(key) + " must be an integer");
  }
  return object.at(key).get<std::int64_t>();
}

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

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace wt::dto {

// Parses a JSON request body. Throws ApiError(invalid request) when the body is
// not valid JSON or not an object.
nlohmann::json parse_object(std::string_view body);

std::string require_string(const nlohmann::json& object, const char* key,
                           std::size_t max_length);
std::optional<std::string> optional_string(const nlohmann::json& object, const char* key,
                                           std::size_t max_length);
std::string optional_string_or(const nlohmann::json& object, const char* key,
                               const std::string& fallback);
std::int64_t require_int64(const nlohmann::json& object, const char* key);
std::optional<std::int64_t> optional_int64(const nlohmann::json& object, const char* key);
bool optional_bool(const nlohmann::json& object, const char* key, bool fallback);

}  // namespace wt::dto

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "crow.h"

#include "common/error.hpp"
#include "common/logging.hpp"
#include "common/response.hpp"

namespace wt::http {

inline crow::response make_json_response(int http_status, std::string body) {
  crow::response response(http_status, std::move(body));
  response.set_header("Content-Type", "application/json; charset=utf-8");
  response.set_header("Access-Control-Allow-Origin", "*");
  response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
  response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  return response;
}

inline crow::response ok(const nlohmann::json& data) {
  return make_json_response(200, success_body(data));
}

inline crow::response fail(int http_status, int code, const std::string& message) {
  return make_json_response(http_status, error_body(code, message));
}

// Executes a handler that returns JSON data, converting exceptions into the
// uniform error envelope.
template <typename Handler>
crow::response handle(Handler&& handler) {
  try {
    return ok(handler());
  } catch (const ApiError& error) {
    return fail(error.http_status(), error.code(), error.what());
  } catch (const std::exception& error) {
    log_error(std::string("unhandled error: ") + error.what());
    return fail(500, error_code::kInternal, "internal server error");
  }
}

inline std::int64_t path_id(const std::string& value) {
  try {
    return std::stoll(value);
  } catch (const std::exception&) {
    throw invalid_request("invalid id in path");
  }
}

inline std::optional<std::string> query_string(const crow::request& request,
                                               const char* key) {
  char* value = request.url_params.get(key);
  if (value == nullptr) {
    return std::nullopt;
  }
  std::string text(value);
  if (text.empty()) {
    return std::nullopt;
  }
  return text;
}

inline std::optional<std::int64_t> query_int64(const crow::request& request,
                                               const char* key) {
  const auto value = query_string(request, key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  try {
    return std::stoll(*value);
  } catch (const std::exception&) {
    throw invalid_request(std::string("query parameter ") + key + " must be an integer");
  }
}

inline int query_int(const crow::request& request, const char* key, int fallback) {
  const auto value = query_int64(request, key);
  return value.has_value() ? static_cast<int>(*value) : fallback;
}

}  // namespace wt::http

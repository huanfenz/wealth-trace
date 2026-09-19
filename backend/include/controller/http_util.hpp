#pragma once

// HTTP 层公共工具：统一 JSON 响应包装、CORS 头、控制器异常处理、
// 路径参数与查询参数解析（查询参数经 request.url_params.get() 读取）。
#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "crow.h"

#include "common/error.hpp"
#include "common/logging.hpp"
#include "common/response.hpp"

namespace wt::http {

// 构造统一 JSON 响应并统一附加 CORS 头（允许任意来源与常用方法与请求头）。
inline crow::response make_json_response(int http_status, std::string body) {
  crow::response response(http_status, std::move(body));
  response.set_header("Content-Type", "application/json; charset=utf-8");
  response.set_header("Access-Control-Allow-Origin", "*");
  response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
  response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  return response;
}

// 成功响应：HTTP 200 + 统一成功包 {"code":0,"message":"success","data":...}。
inline crow::response ok(const nlohmann::json& data) {
  return make_json_response(200, success_body(data));
}

// 失败响应：指定 HTTP 状态码与业务错误码，包体形如
// {"code":ERROR,"message":"...","data":null}。
inline crow::response fail(int http_status, int code, const std::string& message) {
  return make_json_response(http_status, error_body(code, message));
}

// 执行返回 JSON 数据的处理器，并把异常统一转成错误响应：
// ApiError 按其自带 HTTP 状态与业务错误码输出；其他 std::exception 记录日志后
// 返回 500/50002 内部错误。控制器里的业务逻辑应包在此函数内。
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

// 解析字符串形式的路径 id；非数字抛 invalid_request。
inline std::int64_t path_id(const std::string& value) {
  try {
    return std::stoll(value);
  } catch (const std::exception&) {
    throw invalid_request("invalid id in path");
  }
}

// 读取查询参数：参数不存在或为空串均返回 nullopt（空串按未提供处理）。
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

// 读取查询参数并解析为 64 位整数：缺省 / 空串返回 nullopt，非法数字抛
// invalid_request。
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

// 读取整型查询参数，缺省时返回 fallback。
inline int query_int(const crow::request& request, const char* key, int fallback) {
  const auto value = query_int64(request, key);
  return value.has_value() ? static_cast<int>(*value) : fallback;
}

}  // namespace wt::http

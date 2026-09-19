// 业务/API 错误定义：携带应用错误码与对应 HTTP 状态码，并预置常用工厂函数。
#pragma once

#include <stdexcept>
#include <string>

namespace wt {

// 业务/API 级错误异常。`code` 是返回给前端的 JSON 错误码，
// `http_status` 是对应的 HTTP 状态码。
class ApiError : public std::runtime_error {
 public:
  ApiError(int code, int http_status, std::string message)
      : std::runtime_error(message), code_(code), http_status_(http_status) {}

  int code() const noexcept { return code_; }
  int http_status() const noexcept { return http_status_; }

 private:
  int code_;
  int http_status_;
};

// 应用错误码常量：与 HTTP 状态码解耦，前端依据 code 做精确处理。
namespace error_code {
inline constexpr int kOk = 0;
inline constexpr int kInvalidRequest = 40001;
inline constexpr int kNotFound = 40401;
inline constexpr int kConflict = 40901;
inline constexpr int kDatabase = 50001;
inline constexpr int kInternal = 50002;
}  // namespace error_code

// 参数校验失败（400）。
inline ApiError invalid_request(std::string message) {
  return ApiError(error_code::kInvalidRequest, 400, std::move(message));
}

// 资源不存在（404）。
inline ApiError not_found(std::string message) {
  return ApiError(error_code::kNotFound, 404, std::move(message));
}

// 资源冲突（409），如重复创建。
inline ApiError conflict(std::string message) {
  return ApiError(error_code::kConflict, 409, std::move(message));
}

// 数据库层错误（500）。
inline ApiError database_error(std::string message) {
  return ApiError(error_code::kDatabase, 500, std::move(message));
}

}  // namespace wt

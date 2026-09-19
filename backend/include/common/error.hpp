#pragma once

#include <stdexcept>
#include <string>

namespace wt {

// Business/API level error. `code` is the application error code returned in
// the JSON envelope, `http_status` the matching HTTP status code.
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

namespace error_code {
inline constexpr int kOk = 0;
inline constexpr int kInvalidRequest = 40001;
inline constexpr int kNotFound = 40401;
inline constexpr int kConflict = 40901;
inline constexpr int kDatabase = 50001;
inline constexpr int kInternal = 50002;
}  // namespace error_code

inline ApiError invalid_request(std::string message) {
  return ApiError(error_code::kInvalidRequest, 400, std::move(message));
}

inline ApiError not_found(std::string message) {
  return ApiError(error_code::kNotFound, 404, std::move(message));
}

inline ApiError conflict(std::string message) {
  return ApiError(error_code::kConflict, 409, std::move(message));
}

inline ApiError database_error(std::string message) {
  return ApiError(error_code::kDatabase, 500, std::move(message));
}

}  // namespace wt

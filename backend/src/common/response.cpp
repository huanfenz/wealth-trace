// 统一 API 响应体构造实现。
#include "common/response.hpp"

namespace wt {

// 成功响应固定 code=0、message="success"，data 为业务负载。
std::string success_body(const nlohmann::json& data) {
  nlohmann::json body;
  body["code"] = 0;
  body["message"] = "success";
  body["data"] = data;
  return body.dump();
}

// 错误响应 data 固定为 null，便于前端统一判断。
std::string error_body(int code, std::string_view message) {
  nlohmann::json body;
  body["code"] = code;
  body["message"] = std::string(message);
  body["data"] = nullptr;
  return body.dump();
}

}  // namespace wt

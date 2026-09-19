#include "common/response.hpp"

namespace wt {

std::string success_body(const nlohmann::json& data) {
  nlohmann::json body;
  body["code"] = 0;
  body["message"] = "success";
  body["data"] = data;
  return body.dump();
}

std::string error_body(int code, std::string_view message) {
  nlohmann::json body;
  body["code"] = code;
  body["message"] = std::string(message);
  body["data"] = nullptr;
  return body.dump();
}

}  // namespace wt

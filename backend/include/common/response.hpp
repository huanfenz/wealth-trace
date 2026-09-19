// 统一 API 响应体构造：所有接口返回 {"code","message","data"} 三个字段。
#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace wt {

// 构造统一成功响应体：{"code":0,"message":"success","data":...}
std::string success_body(const nlohmann::json& data);

// 无数据的成功响应：data 为空的 JSON 对象。
inline std::string success_body() { return success_body(nlohmann::json::object()); }

// 构造统一错误响应体：{"code":X,"message":"...","data":null}
std::string error_body(int code, std::string_view message);

}  // namespace wt

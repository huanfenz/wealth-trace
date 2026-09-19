#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace wt {

// Builds the uniform success envelope: {"code":0,"message":"success","data":...}
std::string success_body(const nlohmann::json& data);

inline std::string success_body() { return success_body(nlohmann::json::object()); }

// Builds the uniform error envelope: {"code":X,"message":"...","data":null}
std::string error_body(int code, std::string_view message);

}  // namespace wt

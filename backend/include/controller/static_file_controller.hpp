#pragma once

// 静态文件控制器声明：可选地托管前端构建产物（frontend/dist）。
// 因为 API 路由先注册、本控制器后注册，所以 API 始终优先于静态路由。
#include <string>

#include "crow.h"

#include "config/config.hpp"

namespace wt {

class StaticFileController {
 public:
  explicit StaticFileController(FrontendConfig config) : config_(std::move(config)) {}

  // 注册 "/" 与 "/<path>" 的 GET 通配路由（须最后注册）。
  void register_routes(crow::SimpleApp& app);

 private:
  // 读取并返回指定的相对路径文件；未命中时回退 index.html（SPA）。
  crow::response serve(const std::string& relative_path) const;

  FrontendConfig config_;
};

}  // namespace wt

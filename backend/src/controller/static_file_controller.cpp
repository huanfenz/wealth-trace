// 静态文件控制器实现：托管前端构建产物，含目录穿越防护与 SPA 回退。
#include "controller/static_file_controller.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "common/logging.hpp"
#include "controller/http_util.hpp"

namespace wt {
namespace {

namespace fs = std::filesystem;

// 按扩展名返回 Content-Type，未知类型回退 application/octet-stream。
std::string content_type_for(const std::string& extension) {
  if (extension == ".html") return "text/html; charset=utf-8";
  if (extension == ".js") return "text/javascript; charset=utf-8";
  if (extension == ".css") return "text/css; charset=utf-8";
  if (extension == ".json") return "application/json; charset=utf-8";
  if (extension == ".svg") return "image/svg+xml";
  if (extension == ".png") return "image/png";
  if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
  if (extension == ".ico") return "image/x-icon";
  if (extension == ".woff2") return "font/woff2";
  if (extension == ".woff") return "font/woff";
  if (extension == ".map") return "application/json; charset=utf-8";
  return "application/octet-stream";
}

// 目录穿越防护：拒绝绝对路径（以 '/' 开头）和任何包含 ".." 的路径，
// 空路径视为允许（后续会落到 index.html）。
bool is_safe_path(const std::string& relative_path) {
  if (relative_path.empty()) {
    return true;
  }
  if (relative_path.front() == '/' || relative_path.find("..") != std::string::npos) {
    return false;
  }
  return true;
}

}  // namespace

// 解析并返回静态文件：未启用返回 404；路径不安全返回 400；文件不存在时
// 回退到 index.html（SPA 路由）。
crow::response StaticFileController::serve(const std::string& relative_path) const {
  if (!config_.enabled) {
    return http::fail(404, error_code::kNotFound, "not found");
  }
  if (!is_safe_path(relative_path)) {
    return http::fail(400, error_code::kInvalidRequest, "invalid path");
  }

  const fs::path root(config_.dir);
  const std::string requested = relative_path.empty() ? "index.html" : relative_path;
  fs::path file = root / requested;

  std::error_code error;
  if (!fs::is_regular_file(file, error)) {
    // 单页应用回退：未命中的路径一律交给 index.html。
    file = root / "index.html";
    if (!fs::is_regular_file(file, error)) {
      return http::fail(404, error_code::kNotFound,
                        "frontend build not found; run 'npm run build' in frontend/");
    }
  }

  std::ifstream input(file, std::ios::binary);
  if (!input.is_open()) {
    return http::fail(500, error_code::kInternal, "failed to read static file");
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();

  crow::response response(200, buffer.str());
  response.set_header("Content-Type", content_type_for(file.extension().string()));
  return response;
}

void StaticFileController::register_routes(crow::SimpleApp& app) {
  // 根路径直接返回 index.html。
  CROW_ROUTE(app, "/").methods("GET"_method)([this] { return serve("index.html"); });
  // 其余路径按静态文件处理，未命中时由 serve 回退到 index.html。
  CROW_ROUTE(app, "/<path>").methods("GET"_method)(
      [this](const crow::request&, std::string path) { return serve(path); });
}

}  // namespace wt

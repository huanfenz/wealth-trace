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
    // Single page application fallback.
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
  CROW_ROUTE(app, "/").methods("GET"_method)([this] { return serve("index.html"); });
  CROW_ROUTE(app, "/<path>").methods("GET"_method)(
      [this](const crow::request&, std::string path) { return serve(path); });
}

}  // namespace wt

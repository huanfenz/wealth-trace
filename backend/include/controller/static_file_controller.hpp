#pragma once

#include <string>

#include "crow.h"

#include "config/config.hpp"

namespace wt {

// Optionally serves the built frontend (frontend/dist). API routes take
// precedence because they are registered before this catch-all.
class StaticFileController {
 public:
  explicit StaticFileController(FrontendConfig config) : config_(std::move(config)) {}

  void register_routes(crow::SimpleApp& app);

 private:
  crow::response serve(const std::string& relative_path) const;

  FrontendConfig config_;
};

}  // namespace wt

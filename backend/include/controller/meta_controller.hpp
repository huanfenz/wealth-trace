#pragma once

#include <string>
#include <utility>

#include "crow.h"

#include "config/config.hpp"

namespace wt {

class MetaController {
 public:
  explicit MetaController(CategoryConfig categories)
      : categories_(std::move(categories)) {}

  void register_routes(crow::SimpleApp& app);

 private:
  CategoryConfig categories_;
};

}  // namespace wt

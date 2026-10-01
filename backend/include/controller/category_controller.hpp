#pragma once
#include <utility>
#include "crow.h"

#include "controller/auth_middleware.hpp"
#include "service/category_service.hpp"

namespace wt {
class CategoryController {
 public:
  CategoryController(Database& database, CategoryConfig defaults)
      : service_(database, std::move(defaults)) {}
  void register_routes(App& app);
 private:
  CategoryService service_;
};
}  // namespace wt

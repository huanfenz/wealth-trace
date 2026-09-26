#pragma once
#include <utility>
#include "crow.h"
#include "service/category_service.hpp"

namespace wt {
class CategoryController {
 public:
  CategoryController(Database& database, CategoryConfig defaults)
      : service_(database, std::move(defaults)) {}
  void register_routes(crow::SimpleApp& app);
 private:
  CategoryService service_;
};
}  // namespace wt

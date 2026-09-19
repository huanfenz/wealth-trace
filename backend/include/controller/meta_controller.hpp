#pragma once

// 元数据控制器声明：对外暴露枚举取值、默认分类与金额 / 利率单位约定。
#include <string>
#include <utility>

#include "crow.h"

#include "config/config.hpp"

namespace wt {

// 元数据接口：供前端初始化下拉选项与单位换算。
class MetaController {
 public:
  explicit MetaController(CategoryConfig categories)
      : categories_(std::move(categories)) {}

  // 注册 /api/meta 路由。
  void register_routes(crow::SimpleApp& app);

 private:
  CategoryConfig categories_;
};

}  // namespace wt

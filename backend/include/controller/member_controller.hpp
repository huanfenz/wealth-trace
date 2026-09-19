#pragma once

#include "crow.h"

#include "service/member_service.hpp"

namespace wt {

class Database;

class MemberController {
 public:
  explicit MemberController(Database& database) : service_(database) {}

  void register_routes(crow::SimpleApp& app);

 private:
  MemberService service_;
};

}  // namespace wt

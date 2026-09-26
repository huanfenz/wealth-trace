#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace wt {

struct RecurringInvestmentPlan {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  std::int64_t owner_member_id = 0;
  std::optional<std::int64_t> target_asset_id;
  std::optional<std::int64_t> source_asset_id;
  std::int64_t amount = 0;
  std::string frequency;
  std::optional<int> weekday;
  std::optional<int> month_day;
  std::string start_date;
  std::string next_due_date;
  std::string status;
  std::string created_at;
  std::string updated_at;
};

struct RecurringInvestmentExecution {
  std::int64_t id = 0;
  std::int64_t plan_id = 0;
  std::string scheduled_date;
  std::int64_t amount = 0;
  std::optional<std::int64_t> source_asset_id;
  std::optional<std::int64_t> target_asset_id;
  std::string status;
  std::optional<std::int64_t> transaction_id;
  std::optional<std::string> failure_reason;
  std::string created_at;
  std::string updated_at;
};

}  // namespace wt

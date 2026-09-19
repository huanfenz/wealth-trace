#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "common/response.hpp"

namespace {

using nlohmann::json;

TEST(ResponseEnvelope, SuccessHasZeroCodeAndData) {
  const std::string body = wt::success_body(json{{"answer", 42}});
  const auto parsed = json::parse(body);
  EXPECT_EQ(parsed.at("code").get<int>(), 0);
  EXPECT_EQ(parsed.at("message").get<std::string>(), "success");
  EXPECT_EQ(parsed.at("data").at("answer").get<int>(), 42);
}

TEST(ResponseEnvelope, ErrorHasNullData) {
  const std::string body = wt::error_body(40001, "invalid request");
  const auto parsed = json::parse(body);
  EXPECT_EQ(parsed.at("code").get<int>(), 40001);
  EXPECT_EQ(parsed.at("message").get<std::string>(), "invalid request");
  EXPECT_TRUE(parsed.at("data").is_null());
}

}  // namespace

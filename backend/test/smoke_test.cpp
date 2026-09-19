#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "common/response.hpp"

namespace {

using nlohmann::json;

// 验证成功响应信封：code 固定为 0、message 为 "success"，并原样携带 data 负载。
TEST(ResponseEnvelope, SuccessHasZeroCodeAndData) {
  const std::string body = wt::success_body(json{{"answer", 42}});
  const auto parsed = json::parse(body);
  EXPECT_EQ(parsed.at("code").get<int>(), 0);
  EXPECT_EQ(parsed.at("message").get<std::string>(), "success");
  EXPECT_EQ(parsed.at("data").at("answer").get<int>(), 42);
}

// 验证错误响应信封：返回调用方指定的业务错误码与消息，且 data 为 null。
TEST(ResponseEnvelope, ErrorHasNullData) {
  const std::string body = wt::error_body(40001, "invalid request");
  const auto parsed = json::parse(body);
  EXPECT_EQ(parsed.at("code").get<int>(), 40001);
  EXPECT_EQ(parsed.at("message").get<std::string>(), "invalid request");
  EXPECT_TRUE(parsed.at("data").is_null());
}

}  // namespace

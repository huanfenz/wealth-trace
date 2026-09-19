#include <gtest/gtest.h>

#include "common/error.hpp"
#include "utils/money.hpp"
#include "utils/rate.hpp"
#include "utils/time_util.hpp"

namespace {

using namespace wt;

TEST(MoneyTest, FormatsMinorUnits) {
  EXPECT_EQ(money::format(0), "0.00");
  EXPECT_EQ(money::format(5), "0.05");
  EXPECT_EQ(money::format(35), "0.35");
  EXPECT_EQ(money::format(12345), "123.45");
  EXPECT_EQ(money::format(-8000), "-80.00");
  EXPECT_EQ(money::format(-1), "-0.01");
}

TEST(MoneyTest, ParsesYuan) {
  EXPECT_EQ(money::parse_yuan("123.45"), 12345);
  EXPECT_EQ(money::parse_yuan("0.05"), 5);
  EXPECT_EQ(money::parse_yuan("100"), 10000);
  EXPECT_EQ(money::parse_yuan("-80"), -8000);
  EXPECT_EQ(money::parse_yuan(" 12.3 "), 1230);
}

TEST(MoneyTest, RejectsMalformedAmounts) {
  EXPECT_THROW(money::parse_yuan(""), ApiError);
  EXPECT_THROW(money::parse_yuan("abc"), ApiError);
  EXPECT_THROW(money::parse_yuan("1.234"), ApiError);
  EXPECT_THROW(money::parse_yuan("1.2.3"), ApiError);
}

TEST(RateTest, ParsesAndFormats) {
  EXPECT_EQ(rate::parse_percent("1.85"), 18500);
  EXPECT_EQ(rate::parse_ratio("0.0185"), 18500);
  EXPECT_EQ(rate::format_ratio(18500), "0.018500");
  EXPECT_EQ(rate::format_percent(18500), "1.85");
  EXPECT_EQ(rate::format_percent(150000), "15");
  EXPECT_EQ(rate::format_percent(0), "0");
}

TEST(TimeTest, ValidatesFormats) {
  EXPECT_TRUE(time_util::is_valid_datetime("2026-09-19 10:18:58"));
  EXPECT_FALSE(time_util::is_valid_datetime("2026-09-19"));
  EXPECT_FALSE(time_util::is_valid_datetime("2026/09/19 10:18:58"));
  EXPECT_TRUE(time_util::is_valid_date("2029-09-19"));
  EXPECT_FALSE(time_util::is_valid_date("2029-9-19"));
}

}  // namespace

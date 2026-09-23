#include <gtest/gtest.h>

#include "common/error.hpp"
#include "utils/money.hpp"
#include "utils/rate.hpp"
#include "utils/term_date.hpp"
#include "utils/time_util.hpp"

namespace {

using namespace wt;

// 验证金额（单位：分）格式化输出为人民币字符串，含小数与负数处理。
TEST(MoneyTest, FormatsMinorUnits) {
  EXPECT_EQ(money::format(0), "0.00");
  EXPECT_EQ(money::format(5), "0.05");
  EXPECT_EQ(money::format(35), "0.35");
  EXPECT_EQ(money::format(12345), "123.45");
  EXPECT_EQ(money::format(-8000), "-80.00");
  EXPECT_EQ(money::format(-1), "-0.01");
}

// 验证解析「元」字符串为「分」，兼容无小数、负数及首尾空白。
TEST(MoneyTest, ParsesYuan) {
  EXPECT_EQ(money::parse_yuan("123.45"), 12345);
  EXPECT_EQ(money::parse_yuan("0.05"), 5);
  EXPECT_EQ(money::parse_yuan("100"), 10000);
  EXPECT_EQ(money::parse_yuan("-80"), -8000);
  EXPECT_EQ(money::parse_yuan(" 12.3 "), 1230);
}

// 验证非法金额（空串、非数字、超过两位小数、多个小数点）一律抛 ApiError。
TEST(MoneyTest, RejectsMalformedAmounts) {
  EXPECT_THROW(money::parse_yuan(""), ApiError);
  EXPECT_THROW(money::parse_yuan("abc"), ApiError);
  EXPECT_THROW(money::parse_yuan("1.234"), ApiError);
  EXPECT_THROW(money::parse_yuan("1.2.3"), ApiError);
}

// 验证利率百分数与小数比例均按 RATE_SCALE(1000000) 定点解析，并能格式化回原串。
TEST(RateTest, ParsesAndFormats) {
  EXPECT_EQ(rate::parse_percent("1.85"), 18500);
  EXPECT_EQ(rate::parse_ratio("0.0185"), 18500);
  EXPECT_EQ(rate::format_ratio(18500), "0.018500");
  EXPECT_EQ(rate::format_percent(18500), "1.85");
  EXPECT_EQ(rate::format_percent(150000), "15");
  EXPECT_EQ(rate::format_percent(0), "0");
}

// 验证日期时间格式校验：datetime 需 "YYYY-MM-DD HH:MM:SS"，date 需零填充的 "YYYY-MM-DD"。
TEST(TimeTest, ValidatesFormats) {
  EXPECT_TRUE(time_util::is_valid_datetime("2026-09-19 10:18:58"));
  EXPECT_FALSE(time_util::is_valid_datetime("2026-09-19"));
  EXPECT_FALSE(time_util::is_valid_datetime("2026/09/19 10:18:58"));
  EXPECT_TRUE(time_util::is_valid_date("2029-09-19"));
  EXPECT_FALSE(time_util::is_valid_date("2029-9-19"));
}

// 验证日期加减：跨月、跨年、闰年及负数天数均按公历计算。
TEST(TimeTest, AddsDaysAcrossBoundaries) {
  EXPECT_EQ(time_util::add_days("2026-09-22", 90), "2026-12-21");
  EXPECT_EQ(time_util::add_days("2026-12-21", 90), "2027-03-21");
  EXPECT_EQ(time_util::add_days("2026-12-31", 1), "2027-01-01");
  EXPECT_EQ(time_util::add_days("2028-02-28", 1), "2028-02-29");  // 2028 为闰年
  EXPECT_EQ(time_util::add_days("2026-01-01", -1), "2025-12-31");
}

// 业务时区：默认 Asia/Shanghai，业务日期按该时区计算；调度等待秒数落在 (0, 86400]。
TEST(TimeTest, BusinessTimezoneDefaultsToShanghai) {
  time_util::set_business_timezone("Asia/Shanghai");
  EXPECT_EQ(time_util::business_timezone(), "Asia/Shanghai");
  EXPECT_TRUE(time_util::is_valid_date(time_util::business_today()));
  const auto wait = time_util::seconds_until_next_business_midnight();
  EXPECT_GT(wait, 0);
  EXPECT_LE(wait, 86400);
}

// 验证存期日期运算：自然月/自然年推进，月末与闰年落到目标月最后一天。
TEST(TimeTest, AddsCalendarTerms) {
  EXPECT_EQ(time_util::add_months("2026-01-31", 1), "2026-02-28");
  EXPECT_EQ(time_util::add_months("2026-03-31", -1), "2026-02-28");
  EXPECT_EQ(time_util::add_months("2026-09-22", 3), "2026-12-22");
  EXPECT_EQ(time_util::add_years("2024-02-29", 1), "2025-02-28");
  EXPECT_EQ(time_util::add_term("2026-09-22", 3, TermUnit::Year), "2029-09-22");
  EXPECT_EQ(time_util::add_term("2026-01-31", 1, TermUnit::Month), "2026-02-28");
  EXPECT_EQ(time_util::add_term("2026-09-22", 90, TermUnit::Day), "2026-12-21");
}

// 业务时区（Asia/Shanghai，UTC+8）本地时间与 UTC 的相互转换。
TEST(TimeTest, ConvertsBetweenBusinessAndUtc) {
  time_util::set_business_timezone("Asia/Shanghai");
  EXPECT_EQ(time_util::business_to_utc("2026-09-22 08:00:00"), "2026-09-22 00:00:00");
  EXPECT_EQ(time_util::business_to_utc("2026-09-01 00:00:00"), "2026-08-31 16:00:00");
  EXPECT_EQ(time_util::utc_to_business("2026-12-31 16:00:00"), "2027-01-01 00:00:00");
  EXPECT_EQ(time_util::utc_to_business("2026-09-22 00:00:00"), "2026-09-22 08:00:00");
}

// 验证日期相差天数，正负方向与边界。
TEST(TimeTest, ComputesDaysBetween) {
  EXPECT_EQ(time_util::days_between("2026-09-22", "2026-12-21"), 90);
  EXPECT_EQ(time_util::days_between("2026-12-21", "2026-09-22"), -90);
  EXPECT_EQ(time_util::days_between("2026-09-22", "2026-09-22"), 0);
  EXPECT_EQ(time_util::days_between("2028-02-28", "2028-03-01"), 2);
}

}  // namespace

// 时间工具：全项目统一使用 UTC 字符串，日期时间 "YYYY-MM-DD HH:MM:SS"，日期 "YYYY-MM-DD"。
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wt::time_util {

// 时间戳统一以 UTC 存储（"YYYY-MM-DD HH:MM:SS"）；而「业务日期」按业务时区计算。
// 两者用途不同：created_at / transaction_time 等审计时间戳用 UTC，
// 与日期相关的业务判断（债券基金赎回、统计自然月等）用业务日期。

// 审计时间戳统一用 UTC："YYYY-MM-DD HH:MM:SS"。业务日期请用 business_today()。
std::string now_iso8601();

// 设置业务时区（IANA 名称，如 "Asia/Shanghai"）。应在启动、创建线程前调用一次。
void set_business_timezone(std::string_view tz);
// 当前业务时区名称。
std::string business_timezone();

// 当前业务日期 "YYYY-MM-DD"（按业务时区）。
std::string business_today();

// 距离下一个业务时区 0 点的秒数（用于每日维护调度）。
std::int64_t seconds_until_next_business_midnight();

// 业务时区本地时间 -> UTC 时间：把用户选择的本地时间区间换算成查询交易所用的
// UTC 区间（transaction_time 以 UTC 存储）。入参/出参均为 "YYYY-MM-DD HH:MM:SS"。
std::string business_to_utc(std::string_view local_datetime);
// UTC 时间 -> 业务时区本地时间，是 business_to_utc 的逆运算。
std::string utc_to_business(std::string_view utc_datetime);

// 校验格式是否为合法的日期时间/日期（只校验格式，不校验真实历法范围）。
bool is_valid_datetime(std::string_view text);
bool is_valid_date(std::string_view text);

// 合法时原样返回；否则抛出 ApiError(invalid request)。
std::string require_datetime(std::string_view text, std::string_view field);
std::string require_date(std::string_view text, std::string_view field);

// 日期加减：把 "YYYY-MM-DD" 加上（days 可为负）天数，返回同格式日期。
// 只做纯历法运算，不处理节假日/交易日顺延。date 非法时抛 invalid_request。
std::string add_days(std::string_view date, std::int64_t days);

// 两个日期相差的天数（to - from）。任一日期非法时抛 invalid_request。
std::int64_t days_between(std::string_view from, std::string_view to);

}  // namespace wt::time_util

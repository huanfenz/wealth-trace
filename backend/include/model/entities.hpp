#pragma once

// 领域实体：与数据库表一一对应的数据结构。金额一律用最小货币单位整数（人民币「分」，
// 1 元 = 100 分），C++ 用 std::int64_t、SQLite 用 INTEGER；时间统一为 UTC 字符串。

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "model/enums.hpp"

namespace wt {

// 家庭：多租户隔离的顶层单位。
struct Household {
  std::int64_t id = 0;     // 主键
  std::string name;        // 家庭名称
  std::string created_at;  // 创建时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::string updated_at;  // 更新时间，UTC "YYYY-MM-DD HH:MM:SS"
};

// 家庭成员。
struct HouseholdMember {
  std::int64_t id = 0;           // 主键
  std::int64_t household_id = 0; // 所属家庭
  std::string name;              // 成员姓名
  MemberRole role = MemberRole::Member;        // 角色：Owner=户主，Member=普通成员
  MemberStatus status = MemberStatus::Active;  // 状态：Active=启用，Inactive=停用
  std::string created_at;                      // 创建时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::string updated_at;                      // 更新时间，UTC "YYYY-MM-DD HH:MM:SS"
};

// 账户：资产属主的逻辑来源。
struct Account {
  std::int64_t id = 0;              // 主键
  std::int64_t household_id = 0;    // 所属家庭。冗余字段：从属主派生，便于按家庭直接过滤、减少联表
  std::int64_t owner_member_id = 0; // 户主成员。冗余字段：便于按成员过滤/聚合，避免大量联表查询
  std::string name;                 // 账户名称
  AccountType type = AccountType::Bank;         // 账户类型
  std::optional<std::string> institution_name;  // 机构名称（可空）
  std::optional<std::string> account_no_masked; // 脱敏后的账号（可空）
  std::optional<std::string> remark;            // 备注（可空）
  bool enabled = true;                          // 是否启用
  std::string created_at;                       // 创建时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::string updated_at;                       // 更新时间，UTC "YYYY-MM-DD HH:MM:SS"
};

// 资产：余额与流水变化的主体；current_balance 是唯一的当前价值来源。
struct Asset {
  std::int64_t id = 0;              // 主键
  std::int64_t household_id = 0;    // 所属家庭。冗余字段：便于按家庭过滤/统计
  std::int64_t owner_member_id = 0; // 所属成员。冗余字段：来自账户，便于按成员过滤/统计
  std::int64_t account_id = 0;      // 所属账户
  std::string name;                 // 资产名称
  AssetType asset_type = AssetType::Cash;      // 资产类型
  std::int64_t opening_balance = 0;            // 期初金额（分）
  std::int64_t current_balance = 0;            // 当前金额（分）：负债为负值，正资产为非负；
                                               // 仅删除流水并保留余额后可与期初加流水增量不一致
  AssetStatus status = AssetStatus::Active;    // 状态：Active=持有，Closed=已关闭
  std::optional<std::string> remark;           // 备注（可空）
  std::string created_at;                      // 创建时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::string updated_at;                      // 更新时间，UTC "YYYY-MM-DD HH:MM:SS"
};

// 定期存款扩展信息（asset_type = TERM_DEPOSIT 时使用）。
// 本金不再单独保存，统一以 asset.opening_balance 为准。
struct TermDepositDetail {
  std::int64_t asset_id = 0;             // 对应资产 id
  std::int64_t annual_interest_rate = 0; // 年利率，定点整数，RATE_SCALE=1000000（1.85% 存为 18500）
  std::optional<std::string> start_date;    // 起息日 "YYYY-MM-DD"（可空）
  std::optional<std::string> maturity_date; // 到期日 "YYYY-MM-DD"（可空）
  std::optional<std::int64_t> term_value;   // 存期数值（可空）
  std::optional<TermUnit> term_unit;        // 存期单位：Day/Month/Year（可空）
  std::optional<std::string> interest_type; // 计息方式（可空）
  bool auto_rollover = false;               // 是否自动转存
  std::optional<std::string> maturity_action; // 到期处理方式（可空）
};

// 股票基金扩展信息（asset_type = STOCK_FUND 时使用）。名称统一使用 asset.name。
struct StockFundDetail {
  std::int64_t asset_id = 0;                   // 对应资产 id
  std::optional<std::string> fund_code;        // 基金代码（可空）
  std::optional<std::string> lock_start_date;  // 锁定期开始日 "YYYY-MM-DD"（可空）
  std::optional<std::string> lock_end_date;    // 锁定期结束日 "YYYY-MM-DD"（可空）
};

// 债券基金扩展信息（asset_type = BOND_FUND 时使用）。
// 两类债券基金共用，通过 holding_mode 区分：
//   MIN_HOLDING：持有期债基，next_redeem_date 恒为空，first_redeem_date 创建后不变；
//   ROLLING：滚动持有债基，next_redeem_date 由每日维护按 holding_period_days 推进。
// 基金名称统一使用 asset.name；本金统一以 asset.opening_balance 为准。
struct BondFundDetail {
  std::int64_t asset_id = 0;                          // 对应资产 id
  std::optional<std::string> fund_code;               // 基金代码（可空）
  std::optional<std::int64_t> expected_annual_yield_rate;  // 预期年化收益率，定点 RATE_SCALE=1000000（可空）
  std::string purchase_date;                          // 买入/申购确认日期 "YYYY-MM-DD"
  HoldingMode holding_mode = HoldingMode::MinHolding; // 持有方式
  std::int64_t holding_period_days = 0;               // 持有周期（天）
  std::optional<std::string> first_redeem_date;       // 首次可赎回日期 "YYYY-MM-DD"（可空）
  std::optional<std::string> next_redeem_date;        // 下一次可赎回日期，仅 ROLLING（可空）
  std::optional<std::string> maturity_date;           // 产品最终到期日（可空）
};

// 定活理财：申购确认日 + 180/360 自然日为持有期满日；满 30 日后每月 5 日开放转出。
struct FlexibleTermDetail {
  std::int64_t asset_id = 0;
  std::string purchase_date;
  std::int64_t holding_period_days = 180;
};

// 商业养老金：本地业务时间。默认自动续期；预约范围用于提醒，不限制修改。
struct CommercialPensionDetail {
  std::int64_t asset_id = 0;
  std::string purchase_time;                    // YYYY-MM-DD HH:MM:SS，业务时区
  std::int64_t holding_period_value = 0;
  TermUnit holding_period_unit = TermUnit::Year;
  std::optional<std::string> reservation_window_start;
  std::optional<std::string> reservation_window_end;
  bool redeem_at_maturity = false;              // false=到期续期（默认），true=到期赎回
  std::optional<std::string> redeem_at;         // 切换到赎回时锁定的到期时间
};

// 保险扩展信息（asset_type = INSURANCE 时使用）。
struct InsuranceDetail {
  std::int64_t asset_id = 0;                    // 对应资产 id
  std::optional<std::string> policy_no;         // 保单号（可空）
  std::optional<std::string> insurance_company; // 保险公司（可空）
  std::optional<std::string> product_name;      // 产品名称（可空）
  std::optional<std::string> insurance_type;    // 保险类型（可空）
  std::optional<std::string> effective_date;    // 生效日 "YYYY-MM-DD"（可空）
  std::optional<std::string> maturity_date;     // 到期日 "YYYY-MM-DD"（可空）
  std::int64_t annual_premium = 0;              // 年缴保费（分）
  std::int64_t total_paid_premium = 0;          // 累计已缴保费（分）
  std::int64_t insured_amount = 0;              // 保额（分）
  std::optional<std::int64_t> payment_years;    // 缴费年限（可空）
};

// Transaction 表示用户认知中的一笔完整业务。
struct Transaction {
  std::int64_t id = 0;              // 主键
  std::int64_t household_id = 0;    // 所属家庭。冗余字段：便于按家庭过滤/统计
  std::int64_t owner_member_id = 0; // 主要归属成员
  TransactionType type = TransactionType::Expense; // 交易类型
  std::optional<std::int64_t> category_id;         // 收支分类 id（可空）
  std::optional<std::string> category;             // 分类（可空）
  std::optional<InvestmentAction> action;
  std::string transaction_time;     // 交易发生时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::optional<std::string> remark; // 备注（可空）
  TransactionStatus status = TransactionStatus::Normal; // 状态：Normal=有效，Void=已作废
  std::string created_at;           // 创建时间，UTC "YYYY-MM-DD HH:MM:SS"
  std::string updated_at;           // 更新时间，UTC "YYYY-MM-DD HH:MM:SS"
};

// TransactionEntry 表示一笔交易对一项资产造成的价值变化。
struct TransactionEntry {
  std::int64_t id = 0;
  std::int64_t transaction_id = 0;
  std::int64_t household_id = 0;
  std::int64_t owner_member_id = 0;
  std::int64_t asset_id = 0;
  TransactionDirection direction = TransactionDirection::In;
  std::int64_t amount = 0;
  std::optional<std::int64_t> balance_before;
  std::optional<std::int64_t> balance_after;
  std::string created_at;
};

struct TransactionDTO {
  Transaction transaction;
  bool editable = true;
  std::int64_t amount = 0;
  DisplayDirection direction = DisplayDirection::Neutral;
  std::string title;
  std::optional<std::string> subtitle;
  std::optional<std::int64_t> source_asset_id;
  std::optional<std::string> source_asset_name;
  std::optional<std::int64_t> destination_asset_id;
  std::optional<std::string> destination_asset_name;
  std::vector<TransactionEntry> entries;
};

// 家庭维护的收支分类。
struct TransactionCategory {
  std::int64_t id = 0;
  std::int64_t household_id = 0;
  TransactionType type = TransactionType::Expense;
  std::string name;
  int sort_order = 0;
  bool active = true;
  std::string created_at;
  std::string updated_at;
};

}  // namespace wt

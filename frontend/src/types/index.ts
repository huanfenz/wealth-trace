// 领域类型定义：与后端数据模型一一对应；金额字段单位为「分」，利率字段为 1000000 定点整数。
export type MemberRole = 'OWNER' | 'MEMBER' // 成员角色：户主 / 普通成员
export type MemberStatus = 'ACTIVE' | 'INACTIVE' // 成员状态：正常 / 停用
export type AccountType =
  | 'BANK' // 银行
  | 'ALIPAY' // 支付宝
  | 'WECHAT' // 微信
  | 'CASH' // 现金
  | 'SECURITIES' // 证券
  | 'INSURANCE' // 保险
  | 'OTHER' // 其他
export type AssetType =
  | 'CASH' // 现金/活期
  | 'TERM_DEPOSIT' // 定期存款（有 term_deposit 明细）
  | 'STOCK_FUND' // 股票基金（有 stock_fund 明细）
  | 'BOND_FUND' // 债券基金（有 bond_fund 明细）
  | 'FLEXIBLE_TERM' // 定活理财
  | 'COMMERCIAL_PENSION' // 商业养老金
  | 'INSURANCE' // 保险（有 insurance 明细）
  | 'LIABILITY' // 负债
  | 'OTHER' // 其他
export type HoldingMode = 'MIN_HOLDING' | 'ROLLING' // 债券基金持有方式：持有期 / 滚动持有
export type AssetStatus = 'ACTIVE' | 'CLOSED' // 资产状态：有效 / 已关闭
export type TransactionType =
  | 'INCOME' // 收入
  | 'EXPENSE' // 支出
  | 'TRANSFER_IN' // 转账转入
  | 'TRANSFER_OUT' // 转账转出
  | 'ADJUSTMENT' // 余额调整
  | 'ASSET_PURCHASE' // 购入新资产时从付款资产扣款
export type TermUnit = 'DAY' | 'MONTH' | 'YEAR' // 期限单位：天 / 月 / 年

/** 家庭。 */
export interface Household {
  id: number
  name: string
  created_at: string
  updated_at: string
}

/** 家庭成员。 */
export interface Member {
  id: number
  household_id: number // 所属家庭
  name: string
  role: MemberRole
  status: MemberStatus
  created_at: string
  updated_at: string
}

/** 账户：归属某成员，用于归集资产。 */
export interface Account {
  id: number
  household_id: number
  owner_member_id: number // 属主成员
  name: string
  type: AccountType
  institution_name: string | null // 机构名（可空）
  account_no_masked: string | null // 脱敏账号（可空）
  remark: string | null // 备注（可空）
  enabled: boolean // 是否启用
  balance: number // 账户余额（分）
  asset_count: number // 关联资产数量
  created_at: string
  updated_at: string
}

/** 定期存款明细（Asset 的子结构）。 */
export interface TermDepositDetail {
  asset_id: number
  annual_interest_rate: number // 年利率（1000000 定点）
  start_date: string | null // 起息日（可空）
  maturity_date: string | null // 到期日（可空）
  term_value: number | null // 期限数值（可空）
  term_unit: TermUnit | null // 期限单位（可空）
  interest_type: string | null // 计息方式（可空）
  auto_rollover: boolean // 是否自动转存
  maturity_action: string | null // 到期处理方式（可空）
  status: string // 后端按业务日期推导：ACTIVE/MATURED/UNKNOWN
  days_until_maturity: number | null // 距到期天数（后端按业务日期计算，可空）
}

/** 股票基金明细（Asset 的子结构）。 */
export interface StockFundDetail {
  asset_id: number
  fund_code: string | null // 基金代码（可空）
  lock_start_date: string | null // 锁定期开始（可空）
  lock_end_date: string | null // 锁定期结束（可空）
}

/** 债券基金明细（Asset 的子结构）。 */
export interface BondFundDetail {
  asset_id: number
  fund_code: string | null // 基金代码（可空）
  expected_annual_yield_rate: number | null // 预期年化收益率（1000000 定点，可空）
  purchase_date: string // 买入/申购确认日期 YYYY-MM-DD
  holding_mode: HoldingMode // 持有方式
  holding_period_days: number // 持有周期（天）
  first_redeem_date: string | null // 首次可赎回日期（可空）
  next_redeem_date: string | null // 下一次可赎回日期，仅滚动型（可空）
  maturity_date: string | null // 产品最终到期日（可空）
  status: string // 后端按业务日期推导的状态：LOCKED/REDEEMABLE/REDEEMABLE_TODAY/PENDING
  days_until_redeem: number | null // 距可赎回天数（后端按业务日期计算，可空）
}

export interface FlexibleTermDetail {
  asset_id: number
  purchase_date: string
  holding_period_days: 180 | 360
  maturity_date: string
  next_transfer_date: string
  can_transfer: boolean
}

export interface CommercialPensionDetail {
  asset_id: number
  purchase_time: string
  holding_period_value: number
  holding_period_unit: TermUnit
  reservation_window_start: string | null
  reservation_window_end: string | null
  redeem_at_maturity: boolean
  maturity_time: string
  reservation_status: 'NOT_SET' | 'UPCOMING' | 'OPEN' | 'ENDED'
  status: 'ACTIVE' | 'MATURED'
}

/** 保险明细（Asset 的子结构）。 */
export interface InsuranceDetail {
  asset_id: number
  policy_no: string | null // 保单号（可空）
  insurance_company: string | null // 保险公司（可空）
  product_name: string | null // 产品名称（可空）
  insurance_type: string | null // 险种（可空）
  effective_date: string | null // 生效日期（可空）
  maturity_date: string | null // 到期日期（可空）
  annual_premium: number // 年保费（分）
  total_paid_premium: number // 累计已缴保费（分）
  insured_amount: number // 保额（分）
  payment_years: number | null // 缴费年限（可空）
}

/** 资产：归属某账户，按 asset_type 决定携带哪个明细块。 */
export interface Asset {
  id: number
  household_id: number
  owner_member_id: number // 属主成员
  account_id: number // 所属账户
  name: string
  asset_type: AssetType
  opening_balance: number // 初始金额（分）
  current_balance: number // 当前价值（分）
  status: AssetStatus
  remark: string | null // 备注（可空）
  term_deposit: TermDepositDetail | null // 定期存款明细（仅 TERM_DEPOSIT）
  stock_fund: StockFundDetail | null // 股票基金明细（仅 STOCK_FUND）
  bond_fund: BondFundDetail | null // 债券基金明细（仅 BOND_FUND）
  flexible_term: FlexibleTermDetail | null
  commercial_pension: CommercialPensionDetail | null
  insurance: InsuranceDetail | null // 保险明细（仅 INSURANCE）
  created_at: string
  updated_at: string
}

/** 交易流水：收入/支出/转账/调整。 */
export interface Transaction {
  id: number
  household_id: number
  owner_member_id: number // 属主成员
  asset_id: number // 关联资产
  type: TransactionType
  category_id: number | null
  category: string | null // 收支分类（可空）
  amount: number // 金额（分），支出/转出存正值，方向由 type 决定
  transfer_group_id: number | null // 转账分组 ID，配对转入/转出（可空）
  balance_before: number | null // 交易前余额（分，可空）
  balance_after: number | null // 交易后余额（分，可空）
  transaction_time: string
  remark: string | null // 备注（可空）
  status: string // 交易状态
  created_at: string
  updated_at: string
}

export interface TransactionCategory {
  id: number
  household_id: number
  type: 'INCOME' | 'EXPENSE'
  name: string
  sort_order: number
  active: boolean
  created_at: string
  updated_at: string
}

export type InvestmentFrequency = 'DAILY' | 'WEEKLY' | 'BIWEEKLY' | 'MONTHLY'
export interface RecurringInvestmentPlan {
  id: number; household_id: number; owner_member_id: number
  target_asset_id: number | null; source_asset_id: number | null; amount: number
  frequency: InvestmentFrequency; weekday: number | null; month_day: number | null
  start_date: string; next_due_date: string; status: 'ACTIVE' | 'PAUSED' | 'DELETED'
  created_at: string; updated_at: string
}
export interface RecurringInvestmentExecution {
  id: number; plan_id: number; scheduled_date: string; amount: number
  source_asset_id: number | null; target_asset_id: number | null
  status: 'SUCCESS' | 'FAILED' | 'REVERSED'; transfer_group_id: number | null
  failure_reason: string | null; created_at: string; updated_at: string
}

/** 通用「名称 + 金额（分）」统计项。 */
export interface NamedAmount {
  id: number
  name: string
  amount: number // 金额（分）
}

/** 按资产类型汇总的金额项。 */
export interface TypeAmount {
  asset_type: AssetType
  amount: number // 金额（分）
}

/** 按分类汇总的金额项。 */
export interface CategoryAmount {
  category: string
  amount: number // 金额（分）
}

/** 家庭总览统计结果。 */
export interface HouseholdOverview {
  total_assets: number // 总资产（分）
  total_liabilities: number // 总负债（分）
  net_worth: number // 净资产（分）
  month_income: number // 本月收入（分）
  month_expense: number // 本月支出（分）
  month_balance: number // 本月结余（分）
  by_member: NamedAmount[] // 按成员汇总
  by_account: NamedAmount[] // 按账户汇总
  by_type: TypeAmount[] // 按资产类型汇总
}

/** 指定时间区间的收支统计结果。 */
export interface PeriodStatistics {
  from: string
  to: string
  income: number // 区间收入（分）
  expense: number // 区间支出（分）
  balance: number // 区间结余（分）
  by_member: NamedAmount[] // 按成员汇总
  income_categories: CategoryAmount[] // 收入分类汇总
  expense_categories: CategoryAmount[] // 支出分类汇总
}

/** 单月收支趋势项。 */
export interface MonthlyStat {
  month: string // YYYY-MM
  income: number // 收入（分）
  expense: number // 支出（分）
  balance: number // 结余（分）
}

/** 创建资产时的「添加时维护」单条变更：某日期字段推进前后值。 */
export interface CreateMaintenanceChange {
  field: string // next_redeem_date / start_date / maturity_date
  before: string
  after: string
}

/** 创建资产时的「添加时维护」预览结果。 */
export interface CreateMaintenancePreview {
  required: boolean // 是否需要维护
  asset_type: AssetType
  changes: CreateMaintenanceChange[]
}

/** 每日维护单条变更：某个已有资产的日期推进（含资产信息，便于展示）。 */
export interface MaintenanceChange {
  asset_id: number
  asset_name: string // 资产名称
  asset_type: AssetType
  field: string // next_redeem_date / start_date / maturity_date
  before: string
  after: string
}

/** 每日维护预览计划：按当前业务日期推导，不落库。 */
export interface MaintenancePlan {
  business_date: string // 业务日期 YYYY-MM-DD
  required: boolean
  changes: MaintenanceChange[]
}

/** 每日维护执行结果：各类被推进的资产条数。 */
export interface MaintenanceResult {
  business_date: string
  bond_funds: number // 推进的滚动债基数
  term_deposits: number // 续期的自动续存定存数
}

/** 分页结果包装。 */
export interface Paged<T> {
  total: number // 总条数
  items: T[] // 当前页数据
}

/** 全局元数据：各类枚举选项、分类，以及金额/利率的精度约定。 */
export interface Meta {
  member_roles: MemberRole[]
  member_statuses: MemberStatus[]
  account_types: AccountType[]
  asset_types: AssetType[]
  asset_statuses: AssetStatus[]
  transaction_types: TransactionType[]
  transaction_statuses: string[]
  term_units: TermUnit[]
  income_categories: string[]
  expense_categories: string[]
  money: { unit: string; minor_units_per_yuan: number } // 金额单位及每元对应的最小单位数
  rate_scale: number // 利率定点缩放倍数
  business_timezone: string // 业务时区（如 Asia/Shanghai）
  business_date: string // 后端当前业务日期 YYYY-MM-DD
}

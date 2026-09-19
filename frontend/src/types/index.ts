export type MemberRole = 'OWNER' | 'MEMBER'
export type MemberStatus = 'ACTIVE' | 'INACTIVE'
export type AccountType =
  | 'BANK'
  | 'ALIPAY'
  | 'WECHAT'
  | 'CASH'
  | 'SECURITIES'
  | 'INSURANCE'
  | 'OTHER'
export type AssetType =
  | 'CASH'
  | 'TERM_DEPOSIT'
  | 'FUND'
  | 'BOND'
  | 'INSURANCE'
  | 'LIABILITY'
  | 'OTHER'
export type AssetStatus = 'ACTIVE' | 'CLOSED'
export type TransactionType =
  | 'INCOME'
  | 'EXPENSE'
  | 'TRANSFER_IN'
  | 'TRANSFER_OUT'
  | 'ADJUSTMENT'
export type TermUnit = 'DAY' | 'MONTH' | 'YEAR'

export interface Household {
  id: number
  name: string
  created_at: string
  updated_at: string
}

export interface Member {
  id: number
  household_id: number
  name: string
  role: MemberRole
  status: MemberStatus
  created_at: string
  updated_at: string
}

export interface Account {
  id: number
  household_id: number
  owner_member_id: number
  name: string
  type: AccountType
  institution_name: string | null
  account_no_masked: string | null
  remark: string | null
  enabled: boolean
  balance: number
  asset_count: number
  created_at: string
  updated_at: string
}

export interface TermDepositDetail {
  asset_id: number
  principal: number
  annual_interest_rate: number
  start_date: string | null
  maturity_date: string | null
  term_value: number | null
  term_unit: TermUnit | null
  interest_type: string | null
  auto_rollover: boolean
  maturity_action: string | null
}

export interface FundDetail {
  asset_id: number
  fund_code: string | null
  fund_name: string | null
  fund_type: string | null
  lock_start_date: string | null
  lock_end_date: string | null
}

export interface BondDetail {
  asset_id: number
  bond_code: string | null
  bond_name: string | null
  principal: number
  annual_coupon_rate: number
  purchase_date: string | null
  maturity_date: string | null
  lock_end_date: string | null
}

export interface InsuranceDetail {
  asset_id: number
  policy_no: string | null
  insurance_company: string | null
  product_name: string | null
  insurance_type: string | null
  effective_date: string | null
  maturity_date: string | null
  annual_premium: number
  total_paid_premium: number
  insured_amount: number
  payment_years: number | null
}

export interface Asset {
  id: number
  household_id: number
  owner_member_id: number
  account_id: number
  name: string
  asset_type: AssetType
  opening_balance: number
  current_balance: number
  status: AssetStatus
  remark: string | null
  term_deposit: TermDepositDetail | null
  fund: FundDetail | null
  bond: BondDetail | null
  insurance: InsuranceDetail | null
  created_at: string
  updated_at: string
}

export interface Transaction {
  id: number
  household_id: number
  owner_member_id: number
  asset_id: number
  type: TransactionType
  category: string | null
  amount: number
  transfer_group_id: number | null
  balance_before: number | null
  balance_after: number | null
  transaction_time: string
  remark: string | null
  status: string
  created_at: string
  updated_at: string
}

export interface NamedAmount {
  id: number
  name: string
  amount: number
}

export interface TypeAmount {
  asset_type: AssetType
  amount: number
}

export interface CategoryAmount {
  category: string
  amount: number
}

export interface HouseholdOverview {
  total_assets: number
  total_liabilities: number
  net_worth: number
  month_income: number
  month_expense: number
  month_balance: number
  by_member: NamedAmount[]
  by_account: NamedAmount[]
  by_type: TypeAmount[]
}

export interface PeriodStatistics {
  from: string
  to: string
  income: number
  expense: number
  balance: number
  by_member: NamedAmount[]
  income_categories: CategoryAmount[]
  expense_categories: CategoryAmount[]
}

export interface Paged<T> {
  total: number
  items: T[]
}

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
  money: { unit: string; minor_units_per_yuan: number }
  rate_scale: number
}

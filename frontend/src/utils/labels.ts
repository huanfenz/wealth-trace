// 枚举值到中文文案的映射表，供下拉选项与表格展示复用。
import type {
  AccountType,
  AssetStatus,
  AssetType,
  HoldingMode,
  MemberRole,
  MemberStatus,
  TermUnit,
  TransactionType,
} from '@/types'

/** 账户类型中文名。 */
export const accountTypeLabels: Record<AccountType, string> = {
  BANK: '银行',
  ALIPAY: '支付宝',
  WECHAT: '微信',
  CASH: '现金',
  SECURITIES: '证券',
  INSURANCE: '保险',
  OTHER: '其他',
}

/** 资产类型中文名。 */
export const assetTypeLabels: Record<AssetType, string> = {
  CASH: '现金/活期',
  TERM_DEPOSIT: '定期存款',
  STOCK_FUND: '股票基金',
  BOND_FUND: '债券基金',
  FLEXIBLE_TERM: '定活理财',
  COMMERCIAL_PENSION: '商业养老金',
  INSURANCE: '保险',
  LIABILITY: '负债',
  OTHER: '其他',
}

/** 债券基金持有方式中文名。 */
export const holdingModeLabels: Record<HoldingMode, string> = {
  MIN_HOLDING: '持有期',
  ROLLING: '滚动持有',
}

/** 资产状态中文名。 */
export const assetStatusLabels: Record<AssetStatus, string> = {
  ACTIVE: '有效',
  CLOSED: '已关闭',
}

/** 成员角色中文名。 */
export const memberRoleLabels: Record<MemberRole, string> = {
  OWNER: '户主',
  MEMBER: '成员',
}

/** 成员状态中文名。 */
export const memberStatusLabels: Record<MemberStatus, string> = {
  ACTIVE: '正常',
  INACTIVE: '停用',
}

/** 期限单位中文名。 */
export const termUnitLabels: Record<TermUnit, string> = {
  DAY: '天',
  MONTH: '月',
  YEAR: '年',
}

/** 交易类型中文名。 */
export const transactionTypeLabels: Record<TransactionType, string> = {
  INCOME: '收入',
  EXPENSE: '支出',
  TRANSFER_IN: '转入',
  TRANSFER_OUT: '转出',
  ADJUSTMENT: '调整',
  ASSET_PURCHASE: '资产购入',
}

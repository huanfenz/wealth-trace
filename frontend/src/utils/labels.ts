import type {
  AccountType,
  AssetStatus,
  AssetType,
  MemberRole,
  MemberStatus,
  TermUnit,
  TransactionType,
} from '@/types'

export const accountTypeLabels: Record<AccountType, string> = {
  BANK: '银行',
  ALIPAY: '支付宝',
  WECHAT: '微信',
  CASH: '现金',
  SECURITIES: '证券',
  INSURANCE: '保险',
  OTHER: '其他',
}

export const assetTypeLabels: Record<AssetType, string> = {
  CASH: '现金/活期',
  TERM_DEPOSIT: '定期存款',
  FUND: '基金',
  BOND: '债券',
  INSURANCE: '保险',
  LIABILITY: '负债',
  OTHER: '其他',
}

export const assetStatusLabels: Record<AssetStatus, string> = {
  ACTIVE: '有效',
  CLOSED: '已关闭',
}

export const memberRoleLabels: Record<MemberRole, string> = {
  OWNER: '户主',
  MEMBER: '成员',
}

export const memberStatusLabels: Record<MemberStatus, string> = {
  ACTIVE: '正常',
  INACTIVE: '停用',
}

export const termUnitLabels: Record<TermUnit, string> = {
  DAY: '天',
  MONTH: '月',
  YEAR: '年',
}

export const transactionTypeLabels: Record<TransactionType, string> = {
  INCOME: '收入',
  EXPENSE: '支出',
  TRANSFER_IN: '转入',
  TRANSFER_OUT: '转出',
  ADJUSTMENT: '调整',
}

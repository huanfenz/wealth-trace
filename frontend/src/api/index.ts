// 业务 API 集合：按资源（家庭/成员/账户/资产/交易/统计）封装对后端的请求。
import { del, get, post, put } from './http'
import http from './http'
import type {
  Account,
  Asset,
  CreateMaintenancePreview,
  Household,
  HouseholdOverview,
  MaintenancePlan,
  MaintenanceResult,
  Member,
  Meta,
  MonthlyStat,
  Paged,
  PeriodStatistics,
  RecurringInvestmentExecution,
  RecurringInvestmentPlan,
  Transaction,
  TransactionCategory,
} from '@/types'

export async function exportDatabase(): Promise<Blob> {
  const response = await http.get('/database/export', { responseType: 'blob' })
  return response.data as Blob
}

export async function importDatabase(file: File): Promise<{ imported: boolean; schema_version: number }> {
  const response = await http.post('/database/import', file, {
    headers: { 'Content-Type': 'application/vnd.sqlite3' },
  })
  const envelope = response.data
  if (!envelope || envelope.code !== 0) {
    throw new Error(envelope?.message || '数据库导入失败')
  }
  return envelope.data
}

export function listInvestmentPlans(householdId: number): Promise<RecurringInvestmentPlan[]> {
  return get<RecurringInvestmentPlan[]>(`/households/${householdId}/investment-plans`)
}
export function createInvestmentPlan(householdId: number, body: Record<string, unknown>): Promise<RecurringInvestmentPlan> {
  return post<RecurringInvestmentPlan>(`/households/${householdId}/investment-plans`, body)
}
export function updateInvestmentPlan(id: number, body: Record<string, unknown>): Promise<RecurringInvestmentPlan> {
  return put<RecurringInvestmentPlan>(`/investment-plans/${id}`, body)
}
export function setInvestmentPlanStatus(id: number, status: 'ACTIVE' | 'PAUSED'): Promise<RecurringInvestmentPlan> {
  return put<RecurringInvestmentPlan>(`/investment-plans/${id}/status`, { status })
}
export function executeInvestmentPlan(id: number): Promise<RecurringInvestmentExecution> {
  return post<RecurringInvestmentExecution>(`/investment-plans/${id}/execute`, {})
}
export function deleteInvestmentPlan(id: number): Promise<{ deleted: boolean }> {
  return del<{ deleted: boolean }>(`/investment-plans/${id}`)
}
export function listInvestmentExecutions(id: number): Promise<RecurringInvestmentExecution[]> {
  return get<RecurringInvestmentExecution[]>(`/investment-plans/${id}/executions`)
}
export function retryInvestmentExecution(id: number): Promise<RecurringInvestmentExecution> {
  return post<RecurringInvestmentExecution>(`/investment-executions/${id}/retry`, {})
}

/** 获取全局元数据（枚举、分类、金额与利率精度等）。 */
export function getMeta(): Promise<Meta> {
  return get<Meta>('/meta')
}

export function listCategories(householdId: number, type: 'INCOME' | 'EXPENSE', includeInactive = false): Promise<TransactionCategory[]> {
  const query = new URLSearchParams({ type, include_inactive: String(includeInactive) })
  return get<TransactionCategory[]>(`/households/${householdId}/categories?${query}`)
}
export function createCategory(householdId: number, body: { type: 'INCOME' | 'EXPENSE'; name: string }): Promise<TransactionCategory> {
  return post<TransactionCategory>(`/households/${householdId}/categories`, body)
}
export function updateCategory(householdId: number, id: number, body: { name: string; sort_order: number }): Promise<TransactionCategory> {
  return put<TransactionCategory>(`/households/${householdId}/categories/${id}`, body)
}
export function setCategoryActive(householdId: number, id: number, active: boolean): Promise<TransactionCategory> {
  return put<TransactionCategory>(`/households/${householdId}/categories/${id}/status`, { active })
}

// --- households（家庭） ----------------------------------------------------

/** 获取家庭列表。 */
export function listHouseholds(): Promise<Household[]> {
  return get<Household[]>('/households')
}

/** 按 ID 获取单个家庭。 */
export function getHousehold(id: number): Promise<Household> {
  return get<Household>(`/households/${id}`)
}

/** 新建家庭。 */
export function createHousehold(body: Record<string, unknown>): Promise<Household> {
  return post<Household>('/households', body)
}

/** 更新家庭信息。 */
export function updateHousehold(
  id: number,
  body: Record<string, unknown>,
): Promise<Household> {
  return put<Household>(`/households/${id}`, body)
}

// --- members（成员） ------------------------------------------------------

/** 获取指定家庭的成员列表。 */
export function listMembers(householdId: number): Promise<Member[]> {
  return get<Member[]>(`/households/${householdId}/members`)
}

/** 在指定家庭下新建成员。 */
export function createMember(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Member> {
  return post<Member>(`/households/${householdId}/members`, body)
}

/** 更新成员信息。 */
export function updateMember(id: number, body: Record<string, unknown>): Promise<Member> {
  return put<Member>(`/members/${id}`, body)
}

// --- accounts（账户） -----------------------------------------------------

/** 获取账户列表，可按属主成员筛选。 */
export function listAccounts(
  householdId: number,
  ownerMemberId?: number,
): Promise<Account[]> {
  return get<Account[]>(`/households/${householdId}/accounts`, {
    owner_member_id: ownerMemberId,
  })
}

/** 在指定家庭下新建账户。 */
export function createAccount(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Account> {
  return post<Account>(`/households/${householdId}/accounts`, body)
}

/** 更新账户信息。 */
export function updateAccount(
  id: number,
  body: Record<string, unknown>,
): Promise<Account> {
  return put<Account>(`/accounts/${id}`, body)
}

/** 删除账户；账户下仍有资产时后端返回冲突错误。 */
export function deleteAccount(id: number): Promise<null> {
  return del<null>(`/accounts/${id}`)
}

// --- assets（资产） -------------------------------------------------------

/** 获取资产列表，可按成员或账户筛选。 */
export function listAssets(
  householdId: number,
  params: { ownerMemberId?: number; accountId?: number },
): Promise<Asset[]> {
  return get<Asset[]>(`/households/${householdId}/assets`, {
    owner_member_id: params.ownerMemberId,
    account_id: params.accountId,
  })
}

/** 按 ID 获取单个资产（含对应类型的明细块）。 */
export function getAsset(id: number): Promise<Asset> {
  return get<Asset>(`/assets/${id}`)
}

/** 在指定家庭下新建资产。 */
export function createAsset(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Asset> {
  return post<Asset>(`/households/${householdId}/assets`, body)
}

/** 预览新建资产时的「添加时维护」：返回是否需要推进及前后值（不落库）。 */
export function previewCreateMaintenance(
  householdId: number,
  body: Record<string, unknown>,
): Promise<CreateMaintenancePreview> {
  return post<CreateMaintenancePreview>(
    `/households/${householdId}/assets/maintenance-preview`,
    body,
  )
}

/** 更新资产基本信息。 */
export function updateAsset(id: number, body: Record<string, unknown>): Promise<Asset> {
  return put<Asset>(`/assets/${id}`, body)
}

/** 手动设置当前余额，不生成交易记录。 */
export function setAssetBalance(id: number, currentBalance: number): Promise<Asset> {
  return put<Asset>(`/assets/${id}/balance`, { current_balance: currentBalance })
}

/** 更新资产状态（ACTIVE/CLOSED）。 */
export function updateAssetStatus(id: number, status: string): Promise<Asset> {
  return put<Asset>(`/assets/${id}/status`, { status })
}

/** 更新资产对应类型的明细块（定期/股票基金/债券/保险）。 */
export function updateAssetDetail(
  id: number,
  body: Record<string, unknown>,
): Promise<Asset> {
  return put<Asset>(`/assets/${id}/detail`, body)
}

/** 删除资产；其名下全部流水与明细块会被级联删除。 */
export function deleteAsset(id: number): Promise<null> {
  return del<null>(`/assets/${id}`)
}

// --- maintenance（每日维护） ----------------------------------------------

/** 预览每日维护将产生的变更（滚动债基赎回日 / 自动续存存期推进），不落库。 */
export function getMaintenancePreview(): Promise<MaintenancePlan> {
  return get<MaintenancePlan>('/maintenance/preview')
}

/** 强制执行一次每日维护（幂等），返回各类被推进的资产条数。 */
export function runDailyMaintenance(): Promise<MaintenanceResult> {
  return post<MaintenanceResult>('/maintenance/run')
}

// --- transactions（收支与转账） ------------------------------------------

/** 交易列表查询条件。 */
export interface TransactionQuery {
  ownerMemberId?: number // 按成员筛选
  assetId?: number       // 按资产筛选
  type?: string          // 按交易类型筛选
  from?: string          // 起始时间（含）
  to?: string            // 结束时间（含）
  limit?: number         // 每页条数
  offset?: number        // 偏移量（分页）
}

/** 分页查询交易流水。 */
export function listTransactions(
  householdId: number,
  query: TransactionQuery,
): Promise<Paged<Transaction>> {
  return get<Paged<Transaction>>(`/households/${householdId}/transactions`, {
    owner_member_id: query.ownerMemberId,
    asset_id: query.assetId,
    type: query.type,
    from: query.from,
    to: query.to,
    limit: query.limit,
    offset: query.offset,
  })
}

/** 某资产视角的流水，按对应 Entry 的方向展示。 */
export function listAssetTransactions(assetId: number, householdId: number, query: TransactionQuery): Promise<Paged<Transaction>> {
  return get<Paged<Transaction>>(`/assets/${assetId}/transactions`, {
    household_id: householdId,
    owner_member_id: query.ownerMemberId,
    type: query.type,
    from: query.from,
    to: query.to,
    limit: query.limit,
    offset: query.offset,
  })
}

/** 记录一笔收入。 */
export function recordIncome(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/income`, body)
}

/** 记录一笔支出。 */
export function recordExpense(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/expense`, body)
}

/** 记录一次余额调整（金额可为负）。 */
export function recordAdjustment(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/adjustment`, body)
}

/** 在两个资产间转账，后端返回完整业务交易。 */
export function transfer(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transfers`, body)
}

/** 删除流水；可选择是否回滚资产余额，转账流水始终成对删除。 */
export function deleteTransaction(id: number, rollbackAssets = true): Promise<{ deleted: number }> {
  return del<{ deleted: number }>(`/transactions/${id}?rollback_assets=${rollbackAssets}`)
}

/** 获取完整交易及其 Entries，用于编辑回填。 */
export function getTransaction(id: number): Promise<Transaction> {
  return get<Transaction>(`/transactions/${id}`)
}

/** 更新交易；rollbackAssets=false 时保留各资产当前余额。 */
export function updateTransaction(id: number, body: Record<string, unknown>, rollbackAssets = true): Promise<Transaction> {
  return put<Transaction>(`/transactions/${id}?rollback_assets=${rollbackAssets}`, body)
}

/** 修改收入或支出流水的分类；null 表示清空分类。 */
export function updateTransactionCategory(id: number, categoryId: number | null): Promise<Transaction> {
  return put<Transaction>(`/transactions/${id}/category`, { category_id: categoryId })
}

// --- statistics（统计） ---------------------------------------------------

/** 获取家庭总览统计（净资产、收支、按成员/账户/类型汇总）。 */
export function getOverview(
  householdId: number,
  params: { year?: number; month?: number },
): Promise<HouseholdOverview> {
  return get<HouseholdOverview>(`/households/${householdId}/statistics/overview`, {
    year: params.year,
    month: params.month,
  })
}

/** 获取指定时间区间的收支统计，可按成员筛选。 */
export function getPeriod(
  householdId: number,
  params: { from: string; to: string; ownerMemberId?: number },
): Promise<PeriodStatistics> {
  return get<PeriodStatistics>(`/households/${householdId}/statistics/period`, {
    from: params.from,
    to: params.to,
    owner_member_id: params.ownerMemberId,
  })
}

/** 获取近 N 个月收支趋势（按月升序，缺月补零）。 */
export function getMonthlyStats(householdId: number, months: number): Promise<MonthlyStat[]> {
  return get<MonthlyStat[]>(`/households/${householdId}/statistics/monthly`, { months })
}

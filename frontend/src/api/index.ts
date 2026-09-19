import { get, post, put } from './http'
import type {
  Account,
  Asset,
  Household,
  HouseholdOverview,
  Member,
  Meta,
  Paged,
  PeriodStatistics,
  Transaction,
} from '@/types'

export function getMeta(): Promise<Meta> {
  return get<Meta>('/meta')
}

// --- households -----------------------------------------------------------

export function listHouseholds(): Promise<Household[]> {
  return get<Household[]>('/households')
}

export function getHousehold(id: number): Promise<Household> {
  return get<Household>(`/households/${id}`)
}

export function createHousehold(body: Record<string, unknown>): Promise<Household> {
  return post<Household>('/households', body)
}

export function updateHousehold(
  id: number,
  body: Record<string, unknown>,
): Promise<Household> {
  return put<Household>(`/households/${id}`, body)
}

// --- members --------------------------------------------------------------

export function listMembers(householdId: number): Promise<Member[]> {
  return get<Member[]>(`/households/${householdId}/members`)
}

export function createMember(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Member> {
  return post<Member>(`/households/${householdId}/members`, body)
}

export function updateMember(id: number, body: Record<string, unknown>): Promise<Member> {
  return put<Member>(`/members/${id}`, body)
}

// --- accounts -------------------------------------------------------------

export function listAccounts(
  householdId: number,
  ownerMemberId?: number,
): Promise<Account[]> {
  return get<Account[]>(`/households/${householdId}/accounts`, {
    owner_member_id: ownerMemberId,
  })
}

export function createAccount(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Account> {
  return post<Account>(`/households/${householdId}/accounts`, body)
}

export function updateAccount(
  id: number,
  body: Record<string, unknown>,
): Promise<Account> {
  return put<Account>(`/accounts/${id}`, body)
}

// --- assets ---------------------------------------------------------------

export function listAssets(
  householdId: number,
  params: { ownerMemberId?: number; accountId?: number },
): Promise<Asset[]> {
  return get<Asset[]>(`/households/${householdId}/assets`, {
    owner_member_id: params.ownerMemberId,
    account_id: params.accountId,
  })
}

export function getAsset(id: number): Promise<Asset> {
  return get<Asset>(`/assets/${id}`)
}

export function createAsset(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Asset> {
  return post<Asset>(`/households/${householdId}/assets`, body)
}

export function updateAsset(id: number, body: Record<string, unknown>): Promise<Asset> {
  return put<Asset>(`/assets/${id}`, body)
}

export function updateAssetStatus(id: number, status: string): Promise<Asset> {
  return put<Asset>(`/assets/${id}/status`, { status })
}

export function updateAssetDetail(
  id: number,
  body: Record<string, unknown>,
): Promise<Asset> {
  return put<Asset>(`/assets/${id}/detail`, body)
}

// --- transactions ---------------------------------------------------------

export interface TransactionQuery {
  ownerMemberId?: number
  assetId?: number
  type?: string
  from?: string
  to?: string
  limit?: number
  offset?: number
}

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

export function recordIncome(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/income`, body)
}

export function recordExpense(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/expense`, body)
}

export function recordAdjustment(
  householdId: number,
  body: Record<string, unknown>,
): Promise<Transaction> {
  return post<Transaction>(`/households/${householdId}/transactions/adjustment`, body)
}

export function transfer(
  householdId: number,
  body: Record<string, unknown>,
): Promise<{ outgoing: Transaction; incoming: Transaction }> {
  return post<{ outgoing: Transaction; incoming: Transaction }>(
    `/households/${householdId}/transfers`,
    body,
  )
}

// --- statistics -----------------------------------------------------------

export function getOverview(
  householdId: number,
  params: { year?: number; month?: number },
): Promise<HouseholdOverview> {
  return get<HouseholdOverview>(`/households/${householdId}/statistics/overview`, {
    year: params.year,
    month: params.month,
  })
}

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

// 全局 Pinia store：缓存家庭、成员、账户、资产与元数据，并提供刷新与名称查询等能力。
import { defineStore } from 'pinia'
import * as api from '@/api'
import type { Account, Asset, Household, Member, Meta } from '@/types'

export const useAppStore = defineStore('app', {
  state: () => ({
    ready: false,                       // 是否已完成初始化
    household: null as Household | null, // 当前家庭
    members: [] as Member[],            // 成员列表
    meta: null as Meta | null,          // 全局元数据
    accounts: [] as Account[],          // 账户列表
    assets: [] as Asset[],              // 资产列表
  }),
  getters: {
    // 当前家庭 ID；未初始化时返回 0。
    householdId(state): number {
      return state.household?.id ?? 0
    },
    // 由成员 ID 反查名称，找不到时退化为 #id。
    memberName(state) {
      return (id: number): string =>
        state.members.find((member) => member.id === id)?.name ?? `#${id}`
    },
    // 由账户 ID 反查名称。
    accountName(state) {
      return (id: number): string =>
        state.accounts.find((account) => account.id === id)?.name ?? `#${id}`
    },
    // 由资产 ID 反查名称。
    assetName(state) {
      return (id: number): string =>
        state.assets.find((asset) => asset.id === id)?.name ?? `#${id}`
    },
    // 收入可选分类（来自元数据）。
    incomeCategories(state): string[] {
      return state.meta?.income_categories ?? []
    },
    // 支出可选分类（来自元数据）。
    expenseCategories(state): string[] {
      return state.meta?.expense_categories ?? []
    },
  },
  actions: {
    // 首次初始化：加载元数据与家庭，若不存在家庭则自动创建一个，并加载成员。
    async initialize() {
      if (this.ready) {
        return
      }
      this.meta = await api.getMeta()
      const households = await api.listHouseholds()
      this.household =
        households[0] ?? (await api.createHousehold({ name: '我的家庭' }))
      await this.refreshMembers()
      this.ready = true
    },
    // 重新拉取成员列表。
    async refreshMembers() {
      if (!this.household) {
        return
      }
      this.members = await api.listMembers(this.household.id)
    },
    // 重新拉取账户列表。
    async refreshAccounts() {
      if (!this.household) {
        return
      }
      this.accounts = await api.listAccounts(this.household.id)
    },
    // 重新拉取资产列表。
    async refreshAssets() {
      if (!this.household) {
        return
      }
      this.assets = await api.listAssets(this.household.id, {})
    },
    // 并行刷新成员、账户、资产三份缓存。
    async refreshAll() {
      await Promise.all([
        this.refreshMembers(),
        this.refreshAccounts(),
        this.refreshAssets(),
      ])
    },
  },
})

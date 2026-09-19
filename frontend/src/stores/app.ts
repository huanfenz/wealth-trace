import { defineStore } from 'pinia'
import * as api from '@/api'
import type { Account, Asset, Household, Member, Meta } from '@/types'

export const useAppStore = defineStore('app', {
  state: () => ({
    ready: false,
    household: null as Household | null,
    members: [] as Member[],
    meta: null as Meta | null,
    accounts: [] as Account[],
    assets: [] as Asset[],
  }),
  getters: {
    householdId(state): number {
      return state.household?.id ?? 0
    },
    memberName(state) {
      return (id: number): string =>
        state.members.find((member) => member.id === id)?.name ?? `#${id}`
    },
    accountName(state) {
      return (id: number): string =>
        state.accounts.find((account) => account.id === id)?.name ?? `#${id}`
    },
    assetName(state) {
      return (id: number): string =>
        state.assets.find((asset) => asset.id === id)?.name ?? `#${id}`
    },
    incomeCategories(state): string[] {
      return state.meta?.income_categories ?? []
    },
    expenseCategories(state): string[] {
      return state.meta?.expense_categories ?? []
    },
  },
  actions: {
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
    async refreshMembers() {
      if (!this.household) {
        return
      }
      this.members = await api.listMembers(this.household.id)
    },
    async refreshAccounts() {
      if (!this.household) {
        return
      }
      this.accounts = await api.listAccounts(this.household.id)
    },
    async refreshAssets() {
      if (!this.household) {
        return
      }
      this.assets = await api.listAssets(this.household.id, {})
    },
    async refreshAll() {
      await Promise.all([
        this.refreshMembers(),
        this.refreshAccounts(),
        this.refreshAssets(),
      ])
    },
  },
})

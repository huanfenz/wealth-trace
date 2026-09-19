<template>
  <div>
    <el-row :gutter="16">
      <el-col :span="8">
        <el-card shadow="never" class="hero">
          <div class="hero-label">家庭净资产</div>
          <div class="hero-value"><AmountText :value="overview?.net_worth ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">总资产</div>
          <div class="metric-value"><AmountText :value="overview?.total_assets ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">总负债</div>
          <div class="metric-value"><AmountText :value="overview?.total_liabilities ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">本月收入</div>
          <div class="metric-value income"><AmountText :value="overview?.month_income ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">本月支出</div>
          <div class="metric-value expense"><AmountText :value="overview?.month_expense ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">本月结余</div>
          <div class="metric-value"><AmountText :value="overview?.month_balance ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>按成员</span></template>
          <el-table :data="overview?.by_member ?? []" size="small">
            <el-table-column prop="name" label="成员" />
            <el-table-column label="净资产" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>按资产类型</span></template>
          <el-table :data="overview?.by_type ?? []" size="small">
            <el-table-column label="类型">
              <template #default="{ row }">{{ assetTypeLabels[row.asset_type as AssetType] ?? row.asset_type }}</template>
            </el-table-column>
            <el-table-column label="金额" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="never" class="row">
      <template #header><span>按账户</span></template>
      <el-table :data="overview?.by_account ?? []" size="small">
        <el-table-column prop="name" label="账户" />
        <el-table-column label="余额" align="right">
          <template #default="{ row }"><AmountText :value="row.amount" /></template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import { getOverview } from '@/api'
import { useAppStore } from '@/stores/app'
import { assetTypeLabels } from '@/utils/labels'
import type { AssetType, HouseholdOverview } from '@/types'

const store = useAppStore()
const overview = ref<HouseholdOverview | null>(null)

async function load() {
  if (!store.householdId) {
    return
  }
  try {
    overview.value = await getOverview(store.householdId, {})
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
}

onMounted(load)
</script>

<style scoped>
.row {
  margin-top: 16px;
}

.hero {
  background: linear-gradient(135deg, #2f6fed, #4f8cff);
  color: #ffffff;
}

.hero-label {
  font-size: 13px;
  opacity: 0.85;
  margin-bottom: 10px;
}

.hero-value {
  font-size: 28px;
}

.hero-value :deep(.amount) {
  color: #ffffff;
}

.metric-label {
  font-size: 13px;
  color: #909399;
  margin-bottom: 8px;
}

.metric-value {
  font-size: 22px;
}
</style>

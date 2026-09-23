<!-- 家庭总览页：展示净资产/资产负债/本月收支等指标，并以图表与表格汇总资产与收支。 -->
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
          <BaseChart v-if="hasAccounts" :option="accountPieChart" height="300px" />
          <el-empty v-else description="暂无账户数据" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <BaseChart v-if="hasAssetTypes" :option="assetTypeOption" height="300px" />
          <el-empty v-else description="暂无资产数据" />
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="never" class="row">
      <BaseChart v-if="monthly.length" :option="monthlyOption" height="320px" />
      <el-empty v-else description="暂无收支数据" />
    </el-card>

    <el-row :gutter="16" class="row">
      <el-col :span="12">
        <el-card shadow="never">
          <BaseChart v-if="hasAccounts" :option="accountOption" height="320px" />
          <el-empty v-else description="暂无账户数据" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>按账户</span></template>
          <el-table :data="overview?.by_account ?? []" size="small" max-height="320">
            <el-table-column prop="name" label="账户" />
            <el-table-column label="余额" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
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
  </div>
</template>

<script setup lang="ts">
// 职责：进入页面时并行拉取总览与近 6 个月趋势，渲染指标卡、饼图/柱状图/折线图与汇总表。
import { computed, onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import BaseChart from '@/components/BaseChart.vue'
import { getMonthlyStats, getOverview } from '@/api'
import { useAppStore } from '@/stores/app'
import { assetTypeLabels } from '@/utils/labels'
import {
  accountBarOption,
  accountPieOption,
  assetTypePieOption,
  monthlyTrendOption,
} from '@/utils/charts'
import type { AssetType, HouseholdOverview, MonthlyStat } from '@/types'

const store = useAppStore()
const overview = ref<HouseholdOverview | null>(null)
const monthly = ref<MonthlyStat[]>([]) // 近 6 个月收支趋势

// 图表 option：数据为空时对应卡片改用 el-empty 展示。
const assetTypeOption = computed(() => assetTypePieOption(overview.value?.by_type ?? []))
const accountPieChart = computed(() => accountPieOption(overview.value?.by_account ?? []))
const accountOption = computed(() => accountBarOption(overview.value?.by_account ?? []))
const monthlyOption = computed(() => monthlyTrendOption(monthly.value, '近 6 个月收支趋势'))

const hasAssetTypes = computed(() => (overview.value?.by_type.length ?? 0) > 0)
const hasAccounts = computed(() => (overview.value?.by_account.length ?? 0) > 0)

// 并行拉取总览与趋势数据；总览不传 year/month 时后端默认取当前月。
async function load() {
  if (!store.householdId) {
    return
  }
  try {
    const [overviewData, monthlyData] = await Promise.all([
      getOverview(store.householdId, {}),
      getMonthlyStats(store.householdId, 6),
    ])
    overview.value = overviewData
    monthly.value = monthlyData
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

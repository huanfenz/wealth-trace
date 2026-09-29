<!-- 资产总览页：展示净资产/资产负债/本月收支等指标，并以图表汇总资产与收支。 -->
<template>
  <div>
    <el-row :gutter="16">
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never" class="hero">
          <div class="hero-label">家庭净资产</div>
          <div class="hero-value"><AmountText :value="overview?.net_worth ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">总资产</div>
          <div class="metric-value"><AmountText :value="overview?.total_assets ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">总负债</div>
          <div class="metric-value"><AmountText :value="overview?.total_liabilities ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">本月收入</div>
          <div class="metric-value income"><AmountText :value="overview?.month_income ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">本月支出</div>
          <div class="metric-value expense"><AmountText :value="overview?.month_expense ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">本月结余</div>
          <div class="metric-value"><AmountText :value="overview?.month_balance ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="12" :xs="24" :md="12">
        <el-card shadow="never">
          <BaseChart v-if="hasAccounts" :option="accountPieChart" height="300px" @slice-click="openAccountAssets" />
          <el-empty v-else description="暂无账户数据" />
        </el-card>
      </el-col>
      <el-col :span="12" :xs="24" :md="12">
        <el-card shadow="never">
          <BaseChart v-if="hasAssetTypes" :option="assetTypeOption" height="300px" @slice-click="openTypeAssets" />
          <el-empty v-else description="暂无资产数据" />
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="never" class="row">
      <div class="trend-controls">
        <el-radio-group v-model="trendRange" size="small" aria-label="收支趋势时间范围">
          <el-radio-button value="week">近一周</el-radio-button>
          <el-radio-button value="month">近一月</el-radio-button>
          <el-radio-button value="year">近一年</el-radio-button>
        </el-radio-group>
      </div>
      <div v-loading="trendLoading">
        <BaseChart v-if="trend.length" :option="trendOption" height="320px" />
        <el-empty v-else description="暂无收支数据" />
      </div>
    </el-card>
  </div>
</template>

<script setup lang="ts">
// 职责：进入页面时加载总览与收支趋势，渲染指标卡、饼图与折线图。
import { computed, onMounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import BaseChart from '@/components/BaseChart.vue'
import { getOverview, getTrendStats } from '@/api'
import { useAppStore } from '@/stores/app'
import {
  accountPieOption,
  assetTypePieOption,
  incomeExpenseTrendOption,
} from '@/utils/charts'
import type { HouseholdOverview, TrendRange, TrendStat } from '@/types'

const store = useAppStore()
const router = useRouter()
const overview = ref<HouseholdOverview | null>(null)
const trendRange = ref<TrendRange>('year')
const trend = ref<TrendStat[]>([])
const trendLoading = ref(false)
let trendRequestId = 0

// 图表 option：数据为空时对应卡片改用 el-empty 展示。
const assetTypeOption = computed(() => assetTypePieOption(overview.value?.by_type ?? []))
const accountPieChart = computed(() => accountPieOption(overview.value?.by_account ?? []))
const trendTitles: Record<TrendRange, string> = {
  week: '近一周收支趋势',
  month: '近一月收支趋势',
  year: '近一年收支趋势',
}
const trendOption = computed(() => incomeExpenseTrendOption(trend.value, trendTitles[trendRange.value]))

const hasAssetTypes = computed(() => (overview.value?.by_type.length ?? 0) > 0)
const hasAccounts = computed(() => (overview.value?.by_account.length ?? 0) > 0)

function openAccountAssets(index: number) {
  const account = overview.value?.by_account.filter((item) => item.amount !== 0)[index]
  if (account) void router.push({ path: '/assets', query: { account_id: String(account.id) } })
}

function openTypeAssets(index: number) {
  const type = overview.value?.by_type.filter((item) => item.amount !== 0)[index]
  if (type) void router.push({ path: '/assets', query: { asset_type: type.asset_type } })
}

// 总览不传 year/month 时后端默认取当前月。
async function loadOverview() {
  if (!store.householdId) {
    return
  }
  try {
    overview.value = await getOverview(store.householdId, {})
  } catch (error) {
    ElMessage.error((error as Error).message)
  }
}

async function loadTrend() {
  if (!store.householdId) return
  const requestId = ++trendRequestId
  trend.value = []
  trendLoading.value = true
  try {
    const data = await getTrendStats(store.householdId, trendRange.value)
    if (requestId === trendRequestId) trend.value = data
  } catch (error) {
    if (requestId === trendRequestId) ElMessage.error((error as Error).message)
  } finally {
    if (requestId === trendRequestId) trendLoading.value = false
  }
}

watch(trendRange, () => void loadTrend())
onMounted(() => {
  void loadOverview()
  void loadTrend()
})
</script>

<style scoped>
.row {
  margin-top: 16px;
}

.trend-controls {
  display: flex;
  justify-content: flex-end;
  margin-bottom: 8px;
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

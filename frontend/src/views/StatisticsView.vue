<!-- 收支统计页：按月份和成员展示收支、交易活动与分类统计。 -->
<template>
  <div>
    <div class="toolbar">
      <el-date-picker
        v-model="month"
        type="month"
        value-format="YYYY-MM"
        placeholder="选择月份"
        :clearable="false"
        @change="load"
      />
      <el-select v-model="memberId" clearable placeholder="全部成员" style="width: 160px" @change="load">
        <el-option v-for="m in store.members" :key="m.id" :label="m.name" :value="m.id" />
      </el-select>
      <div class="spacer" />
    </div>

    <el-row :gutter="16">
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">收入</div>
          <div class="metric-value income"><AmountText :value="stats?.income ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">支出</div>
          <div class="metric-value expense"><AmountText :value="stats?.expense ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8" :xs="24" :sm="12" :md="8">
        <el-card shadow="never">
          <div class="metric-label">结余</div>
          <div class="metric-value"><AmountText :value="stats?.balance ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <div v-if="store.meta" class="activity-grid row">
      <TransactionHeatmap
        class="activity-heatmap"
        :household-id="store.householdId"
        :business-date="store.meta.business_date"
      />
      <el-card v-loading="statsLoading" shadow="never" class="expense-rank-card">
        <template #header><span class="activity-title"><el-icon><Histogram /></el-icon> 支出分类排行</span></template>
        <div class="expense-rank-list">
          <el-empty v-if="!topExpenseCategories.length" description="该月暂无支出" />
          <div v-for="(item, index) in topExpenseCategories" :key="item.category" class="expense-rank-item">
            <div class="expense-rank-heading">
              <span class="expense-rank-name" :title="item.category">{{ index + 1 }}. {{ item.category }}</span>
              <AmountText :value="item.amount" />
            </div>
            <div class="expense-rank-bar"><span :style="{ width: `${item.amount / topExpenseCategories[0].amount * 100}%` }" /></div>
            <div class="expense-rank-share">{{ (item.amount / (stats?.expense || 1) * 100).toFixed(1) }}%</div>
          </div>
        </div>
      </el-card>
      <TransactionCalendar
        class="activity-calendar"
        :household-id="store.householdId"
        :business-date="`${month}-01`"
      />
    </div>

    <el-row v-loading="statsLoading" :gutter="16" class="row">
      <el-col :span="12" :xs="24" :md="12">
        <el-card shadow="never">
          <BaseChart v-if="hasExpense" :option="expensePieOption" height="300px" @slice-click="(index) => openCategoryTransactions('EXPENSE', index)" />
          <el-empty v-else description="本月暂无支出" />
        </el-card>
      </el-col>
      <el-col :span="12" :xs="24" :md="12">
        <el-card shadow="never">
          <BaseChart v-if="hasIncome" :option="incomePieOption" height="300px" @slice-click="(index) => openCategoryTransactions('INCOME', index)" />
          <el-empty v-else description="本月暂无收入" />
        </el-card>
      </el-col>
    </el-row>

  </div>
</template>

<script setup lang="ts">
// 职责：按所选月份计算起止时间后请求区间统计，渲染收支指标、交易活动与分类图表。
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import BaseChart from '@/components/BaseChart.vue'
import TransactionCalendar from '@/components/TransactionCalendar.vue'
import TransactionHeatmap from '@/components/TransactionHeatmap.vue'
import { getPeriod } from '@/api'
import { useAppStore } from '@/stores/app'
import { categoryPieOption } from '@/utils/charts'
import type { PeriodStatistics } from '@/types'

const store = useAppStore()
const router = useRouter()
// 默认当前月取后端业务日期（业务时区），不用浏览器日期，避免前后端「今天」不一致。
const businessDate = store.meta?.business_date ?? new Date().toISOString().slice(0, 10)
const month = ref(businessDate.slice(0, 7)) // 默认当前月 YYYY-MM
const memberId = ref<number | undefined>(undefined) // 按成员筛选（空为全部）
const stats = ref<PeriodStatistics | null>(null)
const statsLoading = ref(false)
let statsRequestId = 0

// 图表 option 与数据存在性判断。
const expensePieOption = computed(() =>
  categoryPieOption('支出分类', stats.value?.expense_categories ?? []),
)
const incomePieOption = computed(() =>
  categoryPieOption('收入分类', stats.value?.income_categories ?? []),
)
const hasExpense = computed(() => (stats.value?.expense_categories.length ?? 0) > 0)
const hasIncome = computed(() => (stats.value?.income_categories.length ?? 0) > 0)
const topExpenseCategories = computed(() => (stats.value?.expense_categories ?? [])
  .filter((item) => item.amount > 0)
  .sort((a, b) => b.amount - a.amount)
  .slice(0, 3))

function openCategoryTransactions(type: 'INCOME' | 'EXPENSE', index: number) {
  const items = type === 'INCOME' ? stats.value?.income_categories : stats.value?.expense_categories
  const category = items?.filter((item) => item.amount !== 0)[index]
  if (!category) return
  void router.push({
    path: '/transactions',
    query: {
      month: month.value,
      member_id: memberId.value ? String(memberId.value) : undefined,
      type,
      category_id: category.category_id === null ? undefined : String(category.category_id),
      uncategorized: category.category_id === null ? 'true' : undefined,
    },
  })
}

// 把 "YYYY-MM" 换算成该月首日 00:00:00 与末日 23:59:59 的查询区间。
const range = computed(() => {
  const [year, monthValue] = month.value.split('-').map(Number)
  const lastDay = new Date(year, monthValue, 0).getDate() // 下月第 0 天即本月最后一天
  const pad = (value: number) => String(value).padStart(2, '0')
  return {
    from: `${year}-${pad(monthValue)}-01 00:00:00`,
    to: `${year}-${pad(monthValue)}-${pad(lastDay)} 23:59:59`,
  }
})

// 按当前月份区间与成员筛选拉取统计数据。
async function load() {
  if (!store.householdId) {
    return
  }
  const requestId = ++statsRequestId
  stats.value = null
  statsLoading.value = true
  try {
    const result = await getPeriod(store.householdId, {
      from: range.value.from,
      to: range.value.to,
      ownerMemberId: memberId.value,
    })
    if (requestId === statsRequestId) stats.value = result
  } catch (error) {
    if (requestId === statsRequestId) ElMessage.error((error as Error).message)
  } finally {
    if (requestId === statsRequestId) statsLoading.value = false
  }
}

onMounted(load)
</script>

<style scoped>
.row {
  margin-top: 16px;
}

.activity-grid {
  display: grid;
  grid-template-areas: "heatmap" "calendar" "rank";
  grid-template-columns: minmax(0, 1fr);
  gap: 16px;
  align-items: stretch;
}

.activity-grid > * { min-width: 0; }
.activity-heatmap { grid-area: heatmap; }
.activity-calendar { grid-area: calendar; }
.expense-rank-card { grid-area: rank; }
.activity-grid :deep(.el-card) { border-radius: 14px; box-shadow: 0 4px 18px rgb(31 45 61 / 6%); }
.activity-title { display: inline-flex; align-items: center; gap: 8px; font-weight: 600; }
.activity-title .el-icon { color: var(--wt-brand); font-size: 18px; }
.expense-rank-card :deep(.el-card__body) { padding-top: 14px; padding-bottom: 14px; }
.expense-rank-item + .expense-rank-item { margin-top: 10px; }
.expense-rank-heading { display: flex; justify-content: space-between; gap: 10px; font-size: 13px; }
.expense-rank-name { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.expense-rank-heading :deep(.amount) { flex: none; }
.expense-rank-bar { height: 6px; margin-top: 6px; border-radius: 4px; background: #eef1f4; overflow: hidden; }
.expense-rank-bar span { display: block; height: 100%; border-radius: inherit; background: var(--wt-brand); }
.expense-rank-item:nth-child(2) .expense-rank-bar span { background: #6e9cf5; }
.expense-rank-item:nth-child(3) .expense-rank-bar span { background: #a8c7ff; }
.expense-rank-share { margin-top: 3px; color: #909399; font-size: 12px; text-align: right; }

@media (min-width: 1200px) {
  .activity-grid {
    grid-template-areas: "heatmap calendar" "rank calendar";
    grid-template-columns: minmax(0, 2fr) minmax(0, 1.1fr);
    grid-template-rows: repeat(2, minmax(0, 1fr));
  }
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

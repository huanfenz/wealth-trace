<!-- 收支统计页：选择月份（可再按成员筛选）后展示区间收支、分类图表与成员结余。 -->
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
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">收入</div>
          <div class="metric-value income"><AmountText :value="stats?.income ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">支出</div>
          <div class="metric-value expense"><AmountText :value="stats?.expense ?? 0" /></div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never">
          <div class="metric-label">结余</div>
          <div class="metric-value"><AmountText :value="stats?.balance ?? 0" /></div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="12">
        <el-card shadow="never">
          <BaseChart v-if="hasExpense" :option="expensePieOption" height="300px" />
          <el-empty v-else description="本月暂无支出" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <BaseChart v-if="hasIncome" :option="incomePieOption" height="300px" />
          <el-empty v-else description="本月暂无收入" />
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>支出分类</span></template>
          <el-table :data="stats?.expense_categories ?? []" size="small">
            <el-table-column prop="category" label="分类" />
            <el-table-column label="金额" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>收入分类</span></template>
          <el-table :data="stats?.income_categories ?? []" size="small">
            <el-table-column prop="category" label="分类" />
            <el-table-column label="金额" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="row">
      <el-col :span="12">
        <el-card shadow="never">
          <BaseChart v-if="hasMembers" :option="memberBarOption" height="300px" />
          <el-empty v-else description="暂无成员数据" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never">
          <template #header><span>成员结余</span></template>
          <el-table :data="stats?.by_member ?? []" size="small" max-height="300">
            <el-table-column prop="name" label="成员" />
            <el-table-column label="收支结余" align="right">
              <template #default="{ row }"><AmountText :value="row.amount" /></template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
// 职责：按所选月份计算起止时间后请求区间统计，渲染收支指标、分类饼图与成员柱状图/表格。
import { computed, onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import BaseChart from '@/components/BaseChart.vue'
import { getPeriod } from '@/api'
import { useAppStore } from '@/stores/app'
import { categoryPieOption, memberBalanceBarOption } from '@/utils/charts'
import type { PeriodStatistics } from '@/types'

const store = useAppStore()
// 默认当前月取后端业务日期（业务时区），不用浏览器日期，避免前后端「今天」不一致。
const businessDate = store.meta?.business_date ?? new Date().toISOString().slice(0, 10)
const month = ref(businessDate.slice(0, 7)) // 默认当前月 YYYY-MM
const memberId = ref<number | undefined>(undefined) // 按成员筛选（空为全部）
const stats = ref<PeriodStatistics | null>(null)

// 图表 option 与数据存在性判断。
const expensePieOption = computed(() =>
  categoryPieOption('支出分类', stats.value?.expense_categories ?? []),
)
const incomePieOption = computed(() =>
  categoryPieOption('收入分类', stats.value?.income_categories ?? []),
)
const memberBarOption = computed(() => memberBalanceBarOption(stats.value?.by_member ?? []))
const hasExpense = computed(() => (stats.value?.expense_categories.length ?? 0) > 0)
const hasIncome = computed(() => (stats.value?.income_categories.length ?? 0) > 0)
const hasMembers = computed(() => (stats.value?.by_member.length ?? 0) > 0)

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
  try {
    stats.value = await getPeriod(store.householdId, {
      from: range.value.from,
      to: range.value.to,
      ownerMemberId: memberId.value,
    })
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

.metric-label {
  font-size: 13px;
  color: #909399;
  margin-bottom: 8px;
}

.metric-value {
  font-size: 22px;
}
</style>

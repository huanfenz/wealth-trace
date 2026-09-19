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

    <el-card shadow="never" class="row">
      <template #header><span>成员结余</span></template>
      <el-table :data="stats?.by_member ?? []" size="small">
        <el-table-column prop="name" label="成员" />
        <el-table-column label="收支结余" align="right">
          <template #default="{ row }"><AmountText :value="row.amount" /></template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'

import AmountText from '@/components/AmountText.vue'
import { getPeriod } from '@/api'
import { useAppStore } from '@/stores/app'
import type { PeriodStatistics } from '@/types'

const store = useAppStore()
const now = new Date()
const month = ref(`${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}`)
const memberId = ref<number | undefined>(undefined)
const stats = ref<PeriodStatistics | null>(null)

const range = computed(() => {
  const [year, monthValue] = month.value.split('-').map(Number)
  const lastDay = new Date(year, monthValue, 0).getDate()
  const pad = (value: number) => String(value).padStart(2, '0')
  return {
    from: `${year}-${pad(monthValue)}-01 00:00:00`,
    to: `${year}-${pad(monthValue)}-${pad(lastDay)} 23:59:59`,
  }
})

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

<template>
  <el-card shadow="never" class="calendar-card">
    <template #header><span class="activity-title"><el-icon><Calendar /></el-icon>交易日历</span></template>
    <el-calendar v-model="calendarMonth" v-loading="loading">
      <template #header>
        <div class="calendar-navigation">
          <el-button text aria-label="上个月" @click="changeMonth(-1)">‹</el-button>
          <span>{{ displayedMonthLabel }}</span>
          <el-button text aria-label="下个月" @click="changeMonth(1)">›</el-button>
        </div>
      </template>
      <template #date-cell="{ data }">
        <button
          type="button"
          class="calendar-day"
          :class="{ 'other-month': data.type !== 'current-month' }"
          :aria-label="`${data.day}，${counts.get(data.day) ?? 0} 笔交易`"
          @click.stop="openDate(data.day)"
        >
          <span>{{ Number(data.day.slice(-2)) }}</span>
          <small v-if="data.type === 'current-month' && counts.get(data.day)">
            {{ counts.get(data.day) }} 笔
          </small>
        </button>
      </template>
    </el-calendar>
  </el-card>
</template>

<script setup lang="ts">
import dayjs from 'dayjs'
import { computed, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import { getTransactionDays } from '@/api'

const props = defineProps<{ householdId: number; businessDate: string }>()
const router = useRouter()
const calendarMonth = ref(dayjs(props.businessDate).toDate())
const counts = ref(new Map<string, number>())
const loading = ref(false)
let requestId = 0

const displayedMonth = computed(() => dayjs(calendarMonth.value).format('YYYY-MM'))
const displayedMonthLabel = computed(() => dayjs(calendarMonth.value).format('YYYY 年 M 月'))

function changeMonth(step: number) {
  calendarMonth.value = dayjs(calendarMonth.value).add(step, 'month').toDate()
}

async function loadMonth() {
  if (!props.householdId) return
  const currentRequest = ++requestId
  const month = dayjs(`${displayedMonth.value}-01`)
  counts.value = new Map()
  loading.value = true
  try {
    const rows = await getTransactionDays(
      props.householdId,
      month.format('YYYY-MM-DD'),
      month.endOf('month').format('YYYY-MM-DD'),
    )
    if (currentRequest === requestId) counts.value = new Map(rows.map((row) => [row.date, row.count]))
  } catch (error) {
    if (currentRequest === requestId) ElMessage.error((error as Error).message)
  } finally {
    if (currentRequest === requestId) loading.value = false
  }
}

function openDate(date: string) {
  void router.push({ path: '/transactions', query: { date } })
}

watch(() => props.businessDate, (date) => { calendarMonth.value = dayjs(date).toDate() })
watch([displayedMonth, () => props.householdId], () => void loadMonth(), { immediate: true })
</script>

<style scoped>
.activity-title { display: inline-flex; align-items: center; gap: 8px; font-weight: 600; }
.activity-title .el-icon { color: var(--wt-brand); font-size: 18px; }
.calendar-navigation {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  font-size: 14px;
  font-weight: 600;
}
.calendar-navigation :deep(.el-button) { margin: 0; font-size: 20px; }
.calendar-day {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 3px;
  width: 100%;
  height: 100%;
  padding: 5px;
  border: 0;
  background: transparent;
  color: inherit;
  cursor: pointer;
  text-align: left;
}
.calendar-day:hover { background: #f0f5ff; }
.calendar-day.other-month { color: #a8abb2; }
.calendar-day small { color: var(--wt-brand); font-size: 11px; }
.calendar-card :deep(.el-calendar__header) { padding: 0 0 8px; }
.calendar-card :deep(.el-calendar__body) { padding: 4px 0 0; }
.calendar-card :deep(.el-calendar-table .el-calendar-day) { padding: 0; height: 52px; }
@media (max-width: 767px) {
  .calendar-card :deep(.el-calendar-table .el-calendar-day) { height: 48px; }
  .calendar-day { padding: 4px; }
  .calendar-day small { font-size: 10px; }
}
</style>

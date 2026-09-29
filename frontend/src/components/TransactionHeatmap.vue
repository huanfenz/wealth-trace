<template>
  <el-card shadow="never" class="heatmap-card">
    <template #header><span class="activity-title"><el-icon><Calendar /></el-icon>交易日历热力图</span></template>
    <div v-loading="loading" class="heatmap-scroll">
      <div class="heatmap-content" :style="{ '--week-count': weekCount }">
        <div class="month-labels">
          <span v-for="month in monthLabels" :key="month.date" :style="{ left: `${month.column / weekCount * 100}%` }">{{ month.label }}</span>
        </div>
        <div class="heatmap-grid">
          <span v-for="weekday in ['日', '一', '二', '三', '四', '五', '六']" :key="weekday" class="weekday-label">{{ weekday }}</span>
          <template v-for="(day, index) in cells" :key="day?.date ?? `pad-${index}`">
            <span v-if="!day" class="heatmap-pad" />
            <button
              v-else
              type="button"
              class="heatmap-day"
              :class="`level-${level(day.count)}`"
              :title="`${day.date} · ${day.count} 笔交易`"
              :aria-label="`${day.date}，${day.count} 笔交易`"
              @click="openDate(day.date)"
            />
          </template>
        </div>
      </div>
    </div>
    <div class="heatmap-legend">少 <span v-for="step in 5" :key="step" class="legend-swatch" :class="`level-${step - 1}`" /> 多</div>
  </el-card>
</template>

<script setup lang="ts">
import dayjs from 'dayjs'
import { computed, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'

import { getTransactionDays } from '@/api'
import type { DailyTransactionCount } from '@/types'

const props = defineProps<{ householdId: number; businessDate: string }>()
const router = useRouter()
const rows = ref<DailyTransactionCount[]>([])
const loading = ref(false)
let requestId = 0

const start = computed(() => dayjs(props.businessDate).subtract(364, 'day'))
const offset = computed(() => start.value.day())
const weekCount = computed(() => Math.ceil((offset.value + 365) / 7))
const cells = computed<Array<DailyTransactionCount | null>>(() => [
  ...Array<null>(offset.value).fill(null),
  ...rows.value,
])
const monthLabels = computed(() => rows.value.flatMap((row, index) => {
  const date = dayjs(row.date)
  if (date.date() !== 1 && !(index === 0 && date.date() <= 14)) return []
  return [{ date: row.date, label: date.format('M月'), column: Math.floor((offset.value + index) / 7) }]
}))

function level(count: number): number {
  if (count === 0) return 0
  if (count === 1) return 1
  if (count <= 3) return 2
  if (count <= 6) return 3
  return 4
}

async function loadDays() {
  if (!props.householdId || !props.businessDate) return
  const currentRequest = ++requestId
  rows.value = []
  loading.value = true
  try {
    const data = await getTransactionDays(props.householdId, start.value.format('YYYY-MM-DD'), props.businessDate)
    if (currentRequest === requestId) rows.value = data
  } catch (error) {
    if (currentRequest === requestId) ElMessage.error((error as Error).message)
  } finally {
    if (currentRequest === requestId) loading.value = false
  }
}

function openDate(date: string) {
  void router.push({ path: '/transactions', query: { date } })
}

watch([() => props.householdId, () => props.businessDate], () => void loadDays(), { immediate: true })
</script>

<style scoped>
.heatmap-scroll { overflow-x: auto; min-height: 110px; }
.activity-title { display: inline-flex; align-items: center; gap: 8px; font-weight: 600; }
.activity-title .el-icon { color: var(--wt-brand); font-size: 18px; }
.heatmap-content { --grid-gap: 2px; width: 100%; min-width: 0; }
.month-labels { position: relative; width: calc(100% - 14px - var(--grid-gap)); height: 22px; margin-left: calc(14px + var(--grid-gap)); font-size: 11px; color: #606266; }
.month-labels span { position: absolute; white-space: nowrap; }
.heatmap-grid { display: grid; width: 100%; grid-auto-flow: column; grid-template-columns: 14px repeat(var(--week-count), minmax(0, 1fr)); grid-template-rows: repeat(7, auto); gap: var(--grid-gap); }
.weekday-label { display: flex; align-items: center; color: #909399; font-size: 9px; line-height: 1; }
.heatmap-day, .heatmap-pad { display: block; width: 100%; aspect-ratio: 1; height: auto; border: 0; border-radius: 2px; }
.legend-swatch { width: 12px; height: 12px; border: 0; border-radius: 2px; }
.heatmap-day { padding: 0; cursor: pointer; }
.heatmap-day:hover { outline: 2px solid var(--wt-brand); outline-offset: 1px; }
.level-0 { background: #eef3fd; }
.level-1 { background: #dce8ff; }
.level-2 { background: #b9d0ff; }
.level-3 { background: #7fa9f7; }
.level-4 { background: var(--wt-brand); }
.heatmap-legend { display: flex; align-items: center; justify-content: flex-end; gap: 4px; margin-top: 10px; color: #909399; font-size: 12px; }
.legend-swatch { display: inline-block; }
@media (max-width: 767px) {
  .heatmap-content { --grid-gap: 4px; width: 862px; }
  .heatmap-grid { grid-template-columns: 14px repeat(var(--week-count), 12px); }
}
</style>

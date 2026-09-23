<!-- 通用图表容器：封装 ECharts 的初始化、option 更新、容器尺寸自适应与销毁。 -->
<template>
  <div ref="el" class="base-chart" :style="{ height }"></div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as echarts from 'echarts/core'
import { BarChart, LineChart, PieChart } from 'echarts/charts'
import {
  GridComponent,
  LegendComponent,
  TitleComponent,
  TooltipComponent,
} from 'echarts/components'
import { CanvasRenderer } from 'echarts/renderers'
import type { EChartsOption } from 'echarts'

// 按需注册用到的图表与组件，减小打包体积。
echarts.use([
  PieChart,
  LineChart,
  BarChart,
  GridComponent,
  LegendComponent,
  TitleComponent,
  TooltipComponent,
  CanvasRenderer,
])

const props = withDefaults(
  defineProps<{ option: EChartsOption; height?: string }>(),
  { height: '300px' },
)

const el = ref<HTMLDivElement | null>(null)
let chart: ReturnType<typeof echarts.init> | null = null
let observer: ResizeObserver | null = null

// 用 notMerge=true 整体替换，避免切换筛选后残留旧系列。
function render() {
  chart?.setOption(props.option, true)
}

onMounted(() => {
  if (!el.value) {
    return
  }
  chart = echarts.init(el.value)
  render()
  // 侧边栏/窗口尺寸变化时同步图表尺寸。
  observer = new ResizeObserver(() => chart?.resize())
  observer.observe(el.value)
})

watch(() => props.option, render, { deep: true })

onBeforeUnmount(() => {
  observer?.disconnect()
  chart?.dispose()
  chart = null
})
</script>

<style scoped>
.base-chart {
  width: 100%;
}
</style>

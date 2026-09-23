// 图表辅助：把统计聚合转换为 ECharts option，统一配色与「分 -> 元」展示口径。
import type { EChartsOption } from 'echarts'

import { assetTypeLabels } from '@/utils/labels'
import { formatYuan } from '@/utils/money'
import type { AssetType, CategoryAmount, MonthlyStat, NamedAmount, TypeAmount } from '@/types'

// 统一调色板。
export const chartPalette = [
  '#2f6fed',
  '#4f8cff',
  '#67c23a',
  '#e6a23c',
  '#f56c6c',
  '#909399',
  '#9b59b6',
  '#16a085',
  '#d4a017',
  '#5b8ff9',
]

// 分 -> 元（图表展示口径）。
function toYuan(minor: number): number {
  return minor / 100
}

// 金额 tooltip：入参已是「元」，乘 100 还原为「分」再格式化。
function yuanFormatter(value: unknown): string {
  const yuan = Number(value)
  return Number.isFinite(yuan) ? `¥${formatYuan(yuan * 100)}` : '-'
}

// 饼图/环形图公共配置。
function pieOption(title: string, data: { name: string; value: number }[]): EChartsOption {
  return {
    color: chartPalette,
    title: { text: title, left: 'center', textStyle: { fontSize: 14, fontWeight: 600 } },
    tooltip: { trigger: 'item', valueFormatter: yuanFormatter },
    legend: { type: 'scroll', bottom: 0 },
    series: [
      {
        name: title,
        type: 'pie',
        radius: ['42%', '68%'],
        center: ['50%', '48%'],
        itemStyle: { borderRadius: 4, borderColor: '#fff', borderWidth: 2 },
        label: { formatter: '{b}\n{d}%' },
        data,
      },
    ],
  }
}

/** 资产类型分布饼图（取金额绝对值，使负债也能作为一个扇区展示）。 */
export function assetTypePieOption(items: TypeAmount[]): EChartsOption {
  const data = items
    .filter((item) => item.amount !== 0)
    .map((item) => ({
      name: assetTypeLabels[item.asset_type as AssetType] ?? item.asset_type,
      value: toYuan(Math.abs(item.amount)),
    }))
  return pieOption('资产类型分布', data)
}

/** 账户余额分布饼图（取金额绝对值，使负债账户也能作为一个扇区展示）。 */
export function accountPieOption(items: NamedAmount[]): EChartsOption {
  const data = items
    .filter((item) => item.amount !== 0)
    .map((item) => ({ name: item.name, value: toYuan(Math.abs(item.amount)) }))
  return pieOption('账户余额分布', data)
}

/** 收支分类饼图。 */
export function categoryPieOption(title: string, items: CategoryAmount[]): EChartsOption {
  const data = items
    .filter((item) => item.amount !== 0)
    .map((item) => ({ name: item.category, value: toYuan(item.amount) }))
  return pieOption(title, data)
}

/** 近 N 个月收入/支出/结余折线图。 */
export function monthlyTrendOption(
  items: MonthlyStat[],
  title = '收支趋势',
): EChartsOption {
  return {
    color: chartPalette,
    title: { text: title, left: 'center', textStyle: { fontSize: 14, fontWeight: 600 } },
    tooltip: { trigger: 'axis', valueFormatter: yuanFormatter },
    legend: { data: ['收入', '支出', '结余'], bottom: 0 },
    grid: { left: 16, right: 24, top: 48, bottom: 40, containLabel: true },
    xAxis: { type: 'category', boundaryGap: false, data: items.map((item) => item.month) },
    yAxis: { type: 'value' },
    series: [
      { name: '收入', type: 'line', smooth: true, data: items.map((item) => toYuan(item.income)) },
      { name: '支出', type: 'line', smooth: true, data: items.map((item) => toYuan(item.expense)) },
      { name: '结余', type: 'line', smooth: true, data: items.map((item) => toYuan(item.balance)) },
    ],
  }
}

/** 账户余额横向柱状图（允许负值，最多展示前 limit 个）。 */
export function accountBarOption(items: NamedAmount[], limit = 10): EChartsOption {
  const top = items.slice(0, limit)
  // 横向柱状图自下而上绘制，反转后金额大的显示在上方。
  const names = top.map((item) => item.name).reverse()
  const values = top.map((item) => toYuan(item.amount)).reverse()
  return {
    color: chartPalette,
    title: { text: '账户余额', left: 'center', textStyle: { fontSize: 14, fontWeight: 600 } },
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' }, valueFormatter: yuanFormatter },
    grid: { left: 16, right: 32, top: 48, bottom: 16, containLabel: true },
    xAxis: { type: 'value' },
    yAxis: { type: 'category', data: names },
    series: [
      {
        name: '余额',
        type: 'bar',
        barMaxWidth: 24,
        data: values,
        itemStyle: { borderRadius: [0, 4, 4, 0] },
      },
    ],
  }
}

/** 成员结余纵向柱状图（允许负值）。 */
export function memberBalanceBarOption(items: NamedAmount[]): EChartsOption {
  return {
    color: chartPalette,
    title: { text: '成员结余', left: 'center', textStyle: { fontSize: 14, fontWeight: 600 } },
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' }, valueFormatter: yuanFormatter },
    grid: { left: 16, right: 24, top: 48, bottom: 16, containLabel: true },
    xAxis: { type: 'category', data: items.map((item) => item.name) },
    yAxis: { type: 'value' },
    series: [
      {
        name: '结余',
        type: 'bar',
        barMaxWidth: 32,
        data: items.map((item) => toYuan(item.amount)),
      },
    ],
  }
}

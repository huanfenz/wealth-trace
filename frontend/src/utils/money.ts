// 金额与利率换算工具：后端金额以「分」为整数，利率以 1000000 定点整数存储，前端负责二者与「元/百分数」的互转。
const formatter = new Intl.NumberFormat('zh-CN', {
  minimumFractionDigits: 2,
  maximumFractionDigits: 2,
})

/** 将「分」格式化为「元」字符串，如 12345 -> "123.45"。 */
export function formatYuan(minor: number): string {
  if (!Number.isFinite(minor)) {
    return '0.00'
  }
  return formatter.format(minor / 100)
}

/** 将「分」格式化为带人民币符号的字符串，如 12345 -> "¥123.45"。 */
export function formatMoney(minor: number): string {
  return `¥${formatYuan(minor)}`
}

/** 将「元」输入（字符串或数字）换算为「分」整数。 */
export function toMinor(value: string | number): number {
  const numeric = typeof value === 'number' ? value : Number.parseFloat(value)
  if (!Number.isFinite(numeric)) {
    return 0
  }
  return Math.round(numeric * 100)
}

/** 将「分」转换为适合表单回填的「元」字符串（保留两位小数）。 */
export function toYuanInput(minor: number): string {
  return (minor / 100).toFixed(2)
}

/** 将百分数输入（如 1.85）换算为后端定点利率整数（18500）。 */
export function percentToScaled(percent: string | number): number {
  const numeric = typeof percent === 'number' ? percent : Number.parseFloat(percent)
  if (!Number.isFinite(numeric)) {
    return 0
  }
  return Math.round(numeric * 10000)
}

/** 将后端定点利率整数（18500）还原为百分数字符串（"1.85"）。 */
export function scaledToPercent(scaled: number): string {
  const value = scaled / 10000
  return Number.isInteger(value) ? String(value) : value.toFixed(4).replace(/0+$/, '')
}

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

// 严格金额格式：可选负号 + 1~13 位整数 + 可选最多两位小数。
// 拒绝 "1,234.56"、"100元"、"1e3"、"1.234" 等会被 parseFloat 静默截断的输入。
const strictYuanPattern = /^-?\d{1,13}(?:\.\d{1,2})?$/
// 严格百分数格式：同上但允许最多 4 位小数（利率定点为百万分之一）。
const strictPercentPattern = /^-?\d{1,7}(?:\.\d{1,4})?$/

/**
 * 严格解析「元」金额输入为「分」：格式非法（千分位逗号、单位后缀、科学计数、
 * 超过两位小数）或超出安全整数范围时返回 null；0 与负数是否合法由调用方的
 * 业务规则判断。所有用户输入入口应使用本函数而非 toMinor，避免静默记错账。
 */
export function parseYuanToMinor(input: string): number | null {
  const text = input.trim()
  if (!strictYuanPattern.test(text)) {
    return null
  }
  const amount = toMinor(text)
  if (!Number.isSafeInteger(amount)) {
    return null
  }
  return amount
}

/**
 * 严格解析百分数输入为定点利率整数（18500 表示 1.85%）：格式非法或超出
 * 安全整数范围时返回 null。
 */
export function parsePercentToScaled(input: string): number | null {
  const text = input.trim()
  if (!strictPercentPattern.test(text)) {
    return null
  }
  const scaled = percentToScaled(text)
  if (!Number.isSafeInteger(scaled)) {
    return null
  }
  return scaled
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

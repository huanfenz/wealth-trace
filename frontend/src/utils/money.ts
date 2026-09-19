const formatter = new Intl.NumberFormat('zh-CN', {
  minimumFractionDigits: 2,
  maximumFractionDigits: 2,
})

/** Formats minor units (fen) as a yuan string, e.g. 12345 -> "123.45". */
export function formatYuan(minor: number): string {
  if (!Number.isFinite(minor)) {
    return '0.00'
  }
  return formatter.format(minor / 100)
}

/** Formats minor units with a leading CNY marker, e.g. 12345 -> "¥123.45". */
export function formatMoney(minor: number): string {
  return `¥${formatYuan(minor)}`
}

/** Converts a yuan input (string or number) to minor units. */
export function toMinor(value: string | number): number {
  const numeric = typeof value === 'number' ? value : Number.parseFloat(value)
  if (!Number.isFinite(numeric)) {
    return 0
  }
  return Math.round(numeric * 100)
}

/** Converts minor units to a plain yuan string suitable for inputs. */
export function toYuanInput(minor: number): string {
  return (minor / 100).toFixed(2)
}

/** Converts a percent input (e.g. 1.85) to the scaled rate integer (18500). */
export function percentToScaled(percent: string | number): number {
  const numeric = typeof percent === 'number' ? percent : Number.parseFloat(percent)
  if (!Number.isFinite(numeric)) {
    return 0
  }
  return Math.round(numeric * 10000)
}

/** Converts a scaled rate integer (18500) back to a percent string ("1.85"). */
export function scaledToPercent(scaled: number): string {
  const value = scaled / 10000
  return Number.isInteger(value) ? String(value) : value.toFixed(4).replace(/0+$/, '')
}

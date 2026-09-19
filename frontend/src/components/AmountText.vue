<!-- 金额展示组件：接收「分」为单位的数值，按人民币格式化并根据正负着色。 -->
<template>
  <span class="amount" :class="value < 0 ? 'negative' : 'positive'">{{ formatted }}</span>
</template>

<script setup lang="ts">
// value：金额（单位「分」）；formatted：格式化后的 "¥xx.xx" 文本。
import { computed } from 'vue'
import { formatMoney } from '@/utils/money'

const props = defineProps<{ value: number }>()
const formatted = computed(() => formatMoney(props.value))
</script>

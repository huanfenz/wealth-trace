<template>
  <span class="amount" :class="value < 0 ? 'negative' : 'positive'">{{ formatted }}</span>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { formatMoney } from '@/utils/money'

const props = defineProps<{ value: number }>()
const formatted = computed(() => formatMoney(props.value))
</script>

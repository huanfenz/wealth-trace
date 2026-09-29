import { computed, onBeforeUnmount, onMounted, ref, watch, type Ref } from 'vue'

type RecordWithId = { id: number }
type CardId = number | string

/** Coordinates mobile card actions, press feedback, and optional multi-selection. */
export function useMobileCardInteractions<T extends RecordWithId>(selection: Ref<T[]>) {
  const actionCardId = ref<CardId | null>(null)
  const pressedCardId = ref<CardId | null>(null)
  const multiSelectMode = ref(false)
  const selectedIds = computed(() => new Set(selection.value.map((item) => item.id)))

  let pressTimer: number | undefined
  let suppressTimer: number | undefined
  let pointerId: number | null = null
  let pressedId: CardId | null = null
  let startX = 0
  let startY = 0
  let longPressFired = false
  let suppressClickId: CardId | null = null

  function clearPress() {
    if (pressTimer !== undefined) window.clearTimeout(pressTimer)
    pressTimer = undefined
    pointerId = null
  }

  function toggleSelection(item: T) {
    if (selectedIds.value.has(item.id)) {
      selection.value = selection.value.filter((selected) => selected.id !== item.id)
    } else {
      selection.value = [...selection.value, item]
    }
    actionCardId.value = null
    if (selection.value.length === 0) multiSelectMode.value = false
  }

  function onPointerDown(id: CardId, event: PointerEvent, item?: T) {
    if (!event.isPrimary || event.button !== 0) return
    const target = event.target as HTMLElement
    if (target.closest('button, a, input, textarea, select, [role="checkbox"], .el-checkbox, .mobile-record-actions-panel')) return

    clearPress()
    if (suppressTimer !== undefined) window.clearTimeout(suppressTimer)
    pressedId = id
    pressedCardId.value = id
    startX = event.clientX
    startY = event.clientY
    pointerId = event.pointerId
    longPressFired = false
    pressTimer = window.setTimeout(() => {
      pressTimer = undefined
      longPressFired = true
      if (item) {
        actionCardId.value = null
        multiSelectMode.value = true
        if (!selectedIds.value.has(item.id)) selection.value = [...selection.value, item]
      } else {
        actionCardId.value = id
      }
    }, 450)
  }

  function onPointerMove(event: PointerEvent) {
    if (pointerId !== event.pointerId || pressTimer === undefined) return
    if (Math.hypot(event.clientX - startX, event.clientY - startY) > 10) {
      clearPress()
      pressedCardId.value = null
    }
  }

  function onPointerEnd(event: PointerEvent) {
    if (pointerId !== event.pointerId) return
    clearPress()
    pressedCardId.value = null
    if (longPressFired && pressedId !== null) {
      suppressClickId = pressedId
      suppressTimer = window.setTimeout(() => { suppressClickId = null }, 600)
    }
    pressedId = null
  }

  function onCardClick(id: CardId, item?: T) {
    if (suppressClickId === id) {
      suppressClickId = null
      if (suppressTimer !== undefined) window.clearTimeout(suppressTimer)
      suppressTimer = undefined
      return
    }
    if (multiSelectMode.value) {
      if (item) toggleSelection(item)
      return
    }
    actionCardId.value = actionCardId.value === id ? null : id
  }

  function cancelSelection() {
    selection.value = []
    multiSelectMode.value = false
    actionCardId.value = null
  }

  function closeActions() {
    actionCardId.value = null
  }

  function onContextMenu(event: Event) {
    event.preventDefault()
  }

  function onOutsidePointerDown(event: PointerEvent) {
    if (!(event.target as HTMLElement | null)?.closest('.mobile-record-card, .mobile-action-popper')) closeActions()
  }

  watch(() => selection.value.length, (length) => {
    if (length === 0) multiSelectMode.value = false
  })

  onMounted(() => document.addEventListener('pointerdown', onOutsidePointerDown, true))
  onBeforeUnmount(() => {
    clearPress()
    if (suppressTimer !== undefined) window.clearTimeout(suppressTimer)
    document.removeEventListener('pointerdown', onOutsidePointerDown, true)
  })

  return {
    actionCardId,
    pressedCardId,
    multiSelectMode,
    selectedIds,
    onPointerDown,
    onPointerMove,
    onPointerEnd,
    onCardClick,
    onContextMenu,
    toggleSelection,
    cancelSelection,
    closeActions,
  }
}

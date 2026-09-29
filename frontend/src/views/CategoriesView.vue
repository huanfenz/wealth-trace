<template>
  <div class="page-stack">
    <div v-if="multiSelectMode" class="toolbar mobile-only mobile-category-bulk">
      <span class="selection-count">已选 {{ selectedCategories.length }} 项</span>
      <el-button :disabled="saving" @click="setSelectedActive(true)">批量启用</el-button>
      <el-button :disabled="saving" @click="setSelectedActive(false)">批量停用</el-button>
      <el-button @click="cancelSelection">取消多选</el-button>
    </div>
    <el-card v-for="group in groups" :key="group.type">
      <template #header>
        <div class="card-header">
          <span>{{ group.label }}</span>
          <el-button type="primary" :icon="Plus" @click="openCreate(group.type)">新增分类</el-button>
        </div>
      </template>
      <div class="mobile-records">
        <el-empty v-if="!loading && categories.filter((item) => item.type === group.type).length === 0" description="暂无分类" />
        <div v-if="categories.some((item) => item.type === group.type)" class="mobile-actions-hint">轻点显示操作；长按开始多选，再点卡片勾选；点“取消多选”退出。</div>
        <el-card v-for="row in categories.filter((item) => item.type === group.type)" :key="row.id" shadow="never" class="mobile-record-card" :class="{ 'is-pressed': pressedCardId === row.id, 'is-selected': multiSelectMode && selectedIds.has(row.id) }" @pointerdown="onPointerDown(row.id, $event, row)" @pointermove="onPointerMove" @pointerup="onPointerEnd" @pointercancel="onPointerEnd" @click="onCardClick(row.id, row)" @contextmenu="onContextMenu">
          <div class="mobile-record-heading">
            <el-checkbox v-if="multiSelectMode" :model-value="selectedIds.has(row.id)" :aria-label="`选择${row.name}`" @click.stop @change="toggleSelection(row)" />
            <div class="mobile-record-title">{{ row.name }}</div><el-tag :type="row.active ? 'success' : 'info'" size="small">{{ row.active ? '启用' : '停用' }}</el-tag>
          </div>
          <div class="mobile-record-fields"><div class="mobile-record-field"><div class="mobile-record-label">排序</div><div>{{ row.sort_order }}</div></div></div>
          <div v-if="actionCardId === row.id && !multiSelectMode" class="mobile-record-actions-panel" @click.stop="closeActions"><el-button class="mobile-action-button" type="primary" @click="openEdit(row)">编辑</el-button><el-button class="mobile-action-button" :type="row.active ? 'danger' : 'success'" @click="toggle(row)">{{ row.active ? '停用' : '启用' }}</el-button><el-button class="mobile-action-button" type="danger" @click="remove(row)">删除</el-button></div>
        </el-card>
      </div>
      <el-table class="desktop-records" :data="categories.filter((item) => item.type === group.type)" v-loading="loading">
        <el-table-column prop="name" label="分类名称" />
        <el-table-column prop="sort_order" label="排序" width="100" />
        <el-table-column label="状态" width="100">
          <template #default="scope">
            <el-tag :type="scope.row.active ? 'success' : 'info'">{{ scope.row.active ? '启用' : '停用' }}</el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="250" align="right">
          <template #default="scope">
            <el-button link type="primary" @click="openEdit(scope.row)">编辑</el-button>
            <el-button link :type="scope.row.active ? 'danger' : 'success'" @click="toggle(scope.row)">
              {{ scope.row.active ? '停用' : '启用' }}
            </el-button>
            <el-button link type="danger" @click="remove(scope.row)">删除</el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-dialog v-model="dialogVisible" :title="editing ? '编辑分类' : '新增分类'" width="420px">
      <el-form label-width="90px">
        <el-form-item label="收支类型"><el-input :model-value="typeLabel" disabled /></el-form-item>
        <el-form-item label="分类名称" required><el-input v-model="form.name" maxlength="64" /></el-form-item>
        <el-form-item v-if="editing" label="排序"><el-input-number v-model="form.sort_order" :min="0" /></el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="saving" @click="save">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Plus } from '@element-plus/icons-vue'
import { useMobileCardInteractions } from '@/composables/useMobileCardInteractions'
import * as api from '@/api'
import { useAppStore } from '@/stores/app'
import type { TransactionCategory } from '@/types'

const store = useAppStore()
const groups = [{ type: 'INCOME' as const, label: '收入分类' }, { type: 'EXPENSE' as const, label: '支出分类' }]
const categories = ref<TransactionCategory[]>([])
const selectedCategories = ref<TransactionCategory[]>([])
const { actionCardId, pressedCardId, multiSelectMode, selectedIds, onPointerDown, onPointerMove, onPointerEnd, onCardClick, onContextMenu, toggleSelection, cancelSelection, closeActions } = useMobileCardInteractions(selectedCategories)
const loading = ref(false)
const saving = ref(false)
const dialogVisible = ref(false)
const editing = ref(false)
const currentId = ref(0)
const categoryType = ref<'INCOME' | 'EXPENSE'>('INCOME')
const form = reactive({ name: '', sort_order: 0 })
const typeLabel = computed(() => categoryType.value === 'INCOME' ? '收入' : '支出')

async function load() {
  loading.value = true
  try {
    const [income, expense] = await Promise.all([
      api.listCategories(store.householdId, 'INCOME', true),
      api.listCategories(store.householdId, 'EXPENSE', true),
    ])
    categories.value = [...income, ...expense]
  } catch (error) { ElMessage.error((error as Error).message) }
  finally { loading.value = false }
}
function openCreate(type: 'INCOME' | 'EXPENSE') {
  editing.value = false; categoryType.value = type; form.name = ''; form.sort_order = 0; dialogVisible.value = true
}
function openEdit(item: TransactionCategory) {
  editing.value = true; currentId.value = item.id; categoryType.value = item.type
  form.name = item.name; form.sort_order = item.sort_order; dialogVisible.value = true
}
async function save() {
  if (!form.name.trim()) { ElMessage.warning('请输入分类名称'); return }
  saving.value = true
  try {
    if (editing.value) await api.updateCategory(store.householdId, currentId.value, { name: form.name, sort_order: form.sort_order })
    else await api.createCategory(store.householdId, { type: categoryType.value, name: form.name })
    dialogVisible.value = false
    await Promise.all([load(), store.refreshCategories()])
    ElMessage.success('分类已保存')
  } catch (error) { ElMessage.error((error as Error).message) }
  finally { saving.value = false }
}
async function toggle(item: TransactionCategory) {
  try {
    await api.setCategoryActive(store.householdId, item.id, !item.active)
    await Promise.all([load(), store.refreshCategories()])
  } catch (error) { ElMessage.error((error as Error).message) }
}

async function setSelectedActive(active: boolean) {
  const targets = selectedCategories.value.filter((item) => item.active !== active)
  if (!targets.length) { ElMessage.info(active ? '所选分类均已启用' : '所选分类均已停用'); return }
  saving.value = true
  try {
    const results = await Promise.allSettled(targets.map((item) => api.setCategoryActive(store.householdId, item.id, active)))
    await Promise.all([load(), store.refreshCategories()])
    selectedCategories.value = []
    const failed = results.filter((result) => result.status === 'rejected').length
    ElMessage[failed ? 'warning' : 'success'](`批量${active ? '启用' : '停用'}完成：成功 ${targets.length - failed} 项，失败 ${failed} 项`)
  } catch (error) {
    ElMessage.error((error as Error).message)
  } finally {
    saving.value = false
  }
}
async function remove(item: TransactionCategory) {
  try {
    await ElMessageBox.confirm(`确认删除分类“${item.name}”？已被交易使用的分类无法删除。`, '删除分类', {
      type: 'warning', confirmButtonText: '确认删除', confirmButtonClass: 'el-button--danger',
    })
  } catch { return }
  try {
    await api.deleteCategory(store.householdId, item.id)
    await Promise.all([load(), store.refreshCategories()])
    ElMessage.success('分类已删除')
  } catch (error) {
    const message = (error as Error).message
    ElMessage.error(message === 'category is used by transactions' ? '该分类已有交易使用，无法删除。可将其停用。' : message)
  }
}
onMounted(load)
</script>

<style scoped>
.page-stack { display: grid; gap: 16px; }
.card-header { display: flex; align-items: center; justify-content: space-between; }
</style>
